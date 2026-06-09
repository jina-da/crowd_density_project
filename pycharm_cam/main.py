import sys
import cv2          # 영상 처리 (박스 그리기, 화면 출력)
import numpy as np  # 수학 연산 (거리 계산, 배열 처리)
import csv          # CSV 파일 저장
import time         # 타이머 (혼잡 지속 시간 측정)
import itertools    # 중심점 쌍 조합 생성 (모든 쌍 거리 계산용)
from ultralytics import YOLO  # YOLOv8 모델
from datetime import datetime # 현재 시간 기록
from PIL import ImageFont, ImageDraw, Image # 한글 출력
import socket       # TCP/IP 소켓 통신
import json         # JSON 형식 데이터 직렬화
import threading    # 소켓 클라이언트 대기를 별도 스레드로 실행
from sklearn.cluster import DBSCAN  # 밀도 기반 클러스터링 알고리즘

# 한글 폰트 로드 (Windows 기본 볼드 폰트)
font_path = "C:/Windows/Fonts/malgunbd.ttf"
font = ImageFont.truetype(font_path, 25)

# ===== 상수 설정 =====
DIST_SAFE = 7.0         # 전체 평균 거리가 이 값 초과면 GREEN
DIST_CAUTION = 4.5      # 전체 평균 거리가 이 값 미만이면 혼잡 타이머 시작
TIME_DANGER = 10        # 혼잡 상태가 이 시간(초) 이상 지속되면 RED
MIN_FACES = 3           # 탐지 인원이 이 값 미만이면 판단 보류 (PENDING)
DBSCAN_EPS = 120        # DBSCAN: 같은 클러스터로 묶을 최대 픽셀 거리
DBSCAN_MIN = 2          # DBSCAN: 클러스터로 인정할 최소 인원 (3으로 하니 잘 안잡힘)
CLUSTER_CAUTION = 4.0   # 클러스터 내부 평균 거리가 이 값 미만이면 혼잡 클러스터로 판단

# TCP/IP 설정
HOST = '0.0.0.0'    # 모든 IP에서 접속 허용
PORT = 7777
SEND_INTERVAL = 2.0 # MFC로 데이터 전송 간격 (초)

# ===== 소켓 서버 설정 =====
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)  # TCP 소켓 생성
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)  # 포트 재사용 허용 (재실행 시 에러 방지)
server_socket.bind((HOST, PORT))  # 포트 바인딩
server_socket.listen(1)           # 클라이언트 1개 대기
print(f"서버 시작! 포트 {PORT} 대기 중...")

client_socket = None

def wait_for_client():
    global client_socket
    while True:
        try:
            conn, addr = server_socket.accept()  # 클라이언트 접속 대기 (블로킹)
            client_socket = conn
            print(f"클라이언트 연결됨! {addr}")
        except:
            break

# 메인 루프가 멈추지 않도록 별도 스레드에서 클라이언트 대기
threading.Thread(target=wait_for_client, daemon=True).start()

# ===== 모델 로드 =====
model = YOLO('best.pt')  # 파인튜닝된 머리 탐지 모델
print("모델 로드 완료!")


# ===== 함수 정의 =====

# OpenCV 프레임에 한글 텍스트 출력
# OpenCV는 한글 미지원 → PIL로 변환 후 그려서 다시 OpenCV 형식으로 변환
def put_korean_text(frame, text, position, color=(0, 255, 0)):
    img_pil = Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))  # BGR → RGB 변환
    draw = ImageDraw.Draw(img_pil)
    draw.text(position, text, font=font, fill=color)
    return cv2.cvtColor(np.array(img_pil), cv2.COLOR_RGB2BGR)  # RGB → BGR 변환

# YOLO 탐지 결과에서 각 머리 박스의 중심점과 높이 추출
def get_box_centers(results):
    centers = []
    box_heights = []
    for box in results[0].boxes.xyxy:  # xyxy: 좌상단(x1,y1) ~ 우하단(x2,y2) 형식
        x1, y1, x2, y2 = box[:4]
        cx = (x1 + x2) / 2  # 박스 중심 x좌표
        cy = (y1 + y2) / 2  # 박스 중심 y좌표
        centers.append((cx.item(), cy.item()))
        box_heights.append((y2 - y1).item())  # 박스 높이 (카메라 거리 보정에 사용)
    return centers, box_heights

# 모든 머리 중심점 쌍 간 평균/최소 거리 계산
# 박스 높이로 나눠서 카메라 거리에 독립적인 상대값으로 정규화
def calc_avg_distance(centers, box_heights):
    if len(centers) < 2:
        return float('inf'), float('inf')
    avg_box_height = np.mean(box_heights)  # 전체 박스 평균 높이 (정규화 기준)
    distances = [
        np.sqrt((a[0]-b[0])**2 + (a[1]-b[1])**2)  # 두 중심점 간 유클리드 거리
        for a, b in itertools.combinations(centers, 2)  # 모든 쌍 조합
    ]
    avg_dist = np.mean(distances) / avg_box_height  # 정규화된 평균 거리
    min_dist = min(distances) / avg_box_height       # 정규화된 최소 거리 (빨간선 표시용)
    return avg_dist, min_dist

# DBSCAN 클러스터링으로 사람 그룹 탐지 후 각 그룹의 혼잡도 판단
def detect_clusters(centers, box_heights):
    if len(centers) < DBSCAN_MIN:
        return []

    points = np.array(centers)  # DBSCAN 입력: (x, y) 좌표 배열
    db = DBSCAN(eps=DBSCAN_EPS, min_samples=DBSCAN_MIN).fit(points)
    labels = db.labels_  # 각 점의 클러스터 번호 (-1은 노이즈)

    avg_box_height = np.mean(box_heights)
    cluster_info = []

    for label in set(labels):
        if label == -1:  # 노이즈: 어느 클러스터에도 속하지 않는 사람 (혼자 떨어진 사람)
            continue

        mask = labels == label               # 해당 클러스터에 속하는 점만 필터링
        cluster_centers = points[mask]
        cluster_count = len(cluster_centers)

        # 클러스터 내부 평균 거리 계산 (그룹 내 밀집도 측정)
        if cluster_count >= 2:
            inner_distances = [
                np.sqrt((a[0]-b[0])**2 + (a[1]-b[1])**2)
                for a, b in itertools.combinations(cluster_centers, 2)
            ]
            inner_avg_dist = np.mean(inner_distances) / avg_box_height
        else:
            inner_avg_dist = float('inf')

        # 클러스터 내부 평균 거리가 기준 미만이면 혼잡 클러스터로 판단
        is_crowded = inner_avg_dist < CLUSTER_CAUTION

        # 클러스터 전체를 감싸는 영역 박스 좌표 계산 (화면 표시용)
        x_min = int(np.min(cluster_centers[:, 0])) - 20
        y_min = int(np.min(cluster_centers[:, 1])) - 20
        x_max = int(np.max(cluster_centers[:, 0])) + 20
        y_max = int(np.max(cluster_centers[:, 1])) + 20

        cluster_info.append({
            'label': label,
            'count': cluster_count,
            'inner_avg_dist': inner_avg_dist,
            'is_crowded': is_crowded,
            'bbox': (x_min, y_min, x_max, y_max),
            'centers': cluster_centers
        })

    return cluster_info

# 혼잡도 최종 상태 판단
# 혼잡 클러스터 존재 여부를 우선 확인 후, 없으면 전체 평균 거리로 판단
def get_status(avg_dist, elapsed, face_count, cluster_info):
    if face_count < MIN_FACES:
        return "PENDING (< 3 people)"

    crowded_clusters = [c for c in cluster_info if c['is_crowded']]

    # 혼잡 클러스터가 하나라도 있으면 우선적으로 ORANGE/RED 판단
    if crowded_clusters:
        if elapsed >= TIME_DANGER:
            return "RED : Danger"
        else:
            return "ORANGE : Crowd"

    # 혼잡 클러스터 없으면 전체 평균 거리로 판단
    if avg_dist > DIST_SAFE:
        return "GREEN : Safe"
    elif avg_dist > DIST_CAUTION:
        return "YELLOW : Caution"
    elif elapsed < TIME_DANGER:
        return "ORANGE : Crowd"
    else:
        return "RED : Danger"


# ===== CSV 초기화 =====
csv_path = 'crowd_log.csv'
with open(csv_path, 'w', newline='', encoding='utf-8-sig') as f:
    writer = csv.writer(f)
    writer.writerow(['timestamp', 'face_count', 'avg_distance', 'elapsed', 'cluster_count', 'crowded_clusters', 'status'])

# ===== 웹캠 연동 =====
# cap = cv2.VideoCapture(1)  # 웹캠 인덱스 1 (USB 웹캠)
cap = None
for i in range(5):  # 0~4번까지 시도
    temp = cv2.VideoCapture(i)
    if temp.isOpened():
        cap = temp
        print(f"웹캠 연결 성공! (index: {i})")
        break
    temp.release()

if cap is None:
    print("웹캠 연결 실패! 카메라를 확인하세요.")
    sys.exit()  # ← exit() 대신 sys.exit()
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

if not cap.isOpened():
    print("웹캠 연결 실패 !")
    exit()

crowd_start_time = None  # 혼잡 상태 시작 시간 (None이면 비혼잡 상태)
last_send_time = time.time()
last_log_time = time.time()
LOG_INTERVAL = 1.0  # 콘솔 출력 및 CSV 저장 간격 (초)

while True:
    ret, frame = cap.read()
    if not ret:
        break

    # YOLO 추론 (conf=0.5: 신뢰도 50% 이상만 탐지)
    results = model(frame, verbose=False, conf=0.5)
    centers, box_heights = get_box_centers(results)
    avg_dist, min_dist = calc_avg_distance(centers, box_heights)
    face_count = len(centers)

    # DBSCAN 클러스터 탐지
    cluster_info = detect_clusters(centers, box_heights)
    crowded_clusters = [c for c in cluster_info if c['is_crowded']]

    # 혼잡 타이머: 전체 평균 거리 OR 혼잡 클러스터 존재 시 타이머 시작
    if face_count >= MIN_FACES and (avg_dist < DIST_CAUTION or len(crowded_clusters) > 0):
        if crowd_start_time is None:
            crowd_start_time = time.time()
        elapsed = time.time() - crowd_start_time
    else:
        crowd_start_time = None  # 혼잡 해제 시 타이머 초기화
        elapsed = 0.0

    status = get_status(avg_dist, elapsed, face_count, cluster_info)

    # 머리 박스 그리기 (초록색)
    for box in results[0].boxes.xyxy:
        x1, y1, x2, y2 = map(int, box[:4])
        cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)

    # 클러스터 영역 박스 표시 (혼잡: 빨강, 여유: 파랑)
    for cluster in cluster_info:
        x_min, y_min, x_max, y_max = cluster['bbox']
        cluster_color = (0, 0, 255) if cluster['is_crowded'] else (255, 0, 0)  # BGR
        cv2.rectangle(frame, (x_min, y_min), (x_max, y_max), cluster_color, 2)
        label_text = f"G{cluster['label'] + 1}: {cluster['count']}명 {'혼잡' if cluster['is_crowded'] else '여유'}"
        frame = put_korean_text(frame, label_text, (x_min, y_min - 30), color=cluster_color[::-1])  # BGR → RGB 변환

    # 가까운 쌍 사이에 빨간선 표시 (픽셀 거리 기준, min_dist 시각화용)
    for a, b in itertools.combinations(centers, 2):
        dist = np.sqrt((a[0]-b[0])**2 + (a[1]-b[1])**2)
        if dist < DIST_CAUTION:  # 정규화 전 픽셀 거리로 비교 (시각적 표시용)
            cv2.line(frame, (int(a[0]), int(a[1])), (int(b[0]), int(b[1])), (0, 0, 255), 2)

    # 상태별 텍스트 색상 (PIL RGB 기준)
    if "GREEN" in status:
        status_color = (0, 255, 0)
    elif "YELLOW" in status:
        status_color = (255, 255, 0)
    elif "ORANGE" in status:
        status_color = (255, 165, 0)
    elif "RED" in status:
        status_color = (255, 0, 0)
    else:
        status_color = (255, 255, 255)  # PENDING: 흰색

    frame = put_korean_text(frame, f"사람 수 : {face_count}", (10, 30), color=(255, 255, 255))
    frame = put_korean_text(frame, f"평균 거리 : {round(avg_dist, 2) if avg_dist != float('inf') else '--'}", (10, 60), color=(255, 255, 255))
    frame = put_korean_text(frame, f"경과 시간 : {round(elapsed, 1)}s", (10, 90), color=(255, 255, 255))
    frame = put_korean_text(frame, status, (10, 120), color=status_color)
    frame = put_korean_text(frame, f"클러스터 수 : {len(cluster_info)} (혼잡 {len(crowded_clusters)}개)", (10, 150), color=(255, 255, 255))

    # 1초마다 콘솔 출력 + CSV 저장
    if time.time() - last_log_time >= LOG_INTERVAL:
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        print(f"[{timestamp}] 사람 : {face_count} | 거리 : {round(avg_dist,1)} | 경과 : {round(elapsed,1)}s | 클러스터 : {len(cluster_info)}개 혼잡 : {len(crowded_clusters)}개 | {status}")

        with open(csv_path, 'a', newline='', encoding='utf-8-sig') as f:
            writer = csv.writer(f)
            writer.writerow([timestamp, face_count, round(avg_dist, 1), round(elapsed, 1), len(cluster_info), len(crowded_clusters), status])

        last_log_time = time.time()

    # 3초마다 MFC 클라이언트로 JSON 전송
    if time.time() - last_send_time >= SEND_INTERVAL:
        if client_socket:
            try:
                data = {
                    "face_count": face_count,
                    "avg_distance": round(avg_dist, 2) if avg_dist != float('inf') else -1,
                    "elapsed": round(elapsed, 1),
                    "cluster_count": len(cluster_info),
                    "crowded_clusters": len(crowded_clusters),
                    "status": status
                }
                msg = json.dumps(data) + '\n'  # '\n'으로 데이터 끝 구분
                client_socket.sendall(msg.encode('utf-8'))
            except:
                client_socket = None
                print("클라이언트 연결 끊김!")
        last_send_time = time.time()

    cv2.imshow('Crowd Detection', frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):  # q 키 누르면 종료
        break

server_socket.close()
cap.release()
cv2.destroyAllWindows()
print("종료!")