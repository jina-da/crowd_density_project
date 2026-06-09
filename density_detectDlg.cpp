#include "pch.h"
#include "framework.h"
#include "density_detect.h"
#include "density_detectDlg.h"
#include "afxdialogex.h"
#include <afxsock.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ─────────────────────────────────────────────────────────────────────────────
// About 다이얼로그 (기존 그대로)
// ─────────────────────────────────────────────────────────────────────────────
class CAboutDlg : public CDialogEx
{
public:
    CAboutDlg();
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_ABOUTBOX };
#endif
protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()
};
CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX) {}
void CAboutDlg::DoDataExchange(CDataExchange* pDX) { CDialogEx::DoDataExchange(pDX); }
BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// ─────────────────────────────────────────────────────────────────────────────
// CClientSocket – OnReceive / OnClose / OnConnect
// ─────────────────────────────────────────────────────────────────────────────
void CClientSocket::OnReceive(int /*nErrorCode*/)
{
    char buf[4096] = {};
    int received = Receive(buf, sizeof(buf) - 1);
    if (received <= 0) return;

    m_recvBuf += CString(CA2W(buf, CP_UTF8));

    int pos;
    while ((pos = m_recvBuf.Find(_T('\n'))) != -1)
    {
        CString line = m_recvBuf.Left(pos).Trim();
        m_recvBuf = m_recvBuf.Mid(pos + 1);

        if (!line.IsEmpty() && m_pDlg)
        {
            CString* pLine = new CString(line);
            m_pDlg->PostMessage(WM_SOCKET_RECEIVED, 0, (LPARAM)pLine);
        }
    }
}

void CClientSocket::OnClose(int /*nErrorCode*/)
{
    Close();
    if (m_pDlg)
        m_pDlg->PostMessage(WM_SOCKET_CLOSED, 0, 0);
}

void CClientSocket::OnConnect(int nErrorCode)
{
    m_pDlg->PostMessage(WM_SOCKET_CONNECTED, (WPARAM)nErrorCode, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// 메시지 맵
// ─────────────────────────────────────────────────────────────────────────────
BEGIN_MESSAGE_MAP(CdensitydetectDlg, CDialogEx)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_DESTROY()
    ON_WM_CTLCOLOR()
    ON_BN_CLICKED(IDC_BTN_CONNECT, &CdensitydetectDlg::OnBtnConnect)
    ON_BN_CLICKED(IDC_BTN_DISCONNECT, &CdensitydetectDlg::OnBtnDisconnect)
    ON_MESSAGE(WM_SOCKET_RECEIVED, &CdensitydetectDlg::OnSocketReceived)
    ON_MESSAGE(WM_SOCKET_CLOSED, &CdensitydetectDlg::OnSocketClosed)
    ON_MESSAGE(WM_SOCKET_CONNECTED, &CdensitydetectDlg::OnSocketConnected)
END_MESSAGE_MAP()

// ─────────────────────────────────────────────────────────────────────────────
// 생성자 / DoDataExchange
// ─────────────────────────────────────────────────────────────────────────────
CdensitydetectDlg::CdensitydetectDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_DENSITY_DETECT_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CdensitydetectDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_SERVER, m_editServer);
    DDX_Control(pDX, IDC_EDIT_PORT, m_editPort);
    DDX_Control(pDX, IDC_BTN_CONNECT, m_btnConnect);
    DDX_Control(pDX, IDC_BTN_DISCONNECT, m_btnDisconnect);
    DDX_Control(pDX, IDC_LBL_CONN_STATUS, m_lblConnStatus);
    DDX_Control(pDX, IDC_LBL_STATUS_BOX, m_lblStatusBox);
    DDX_Control(pDX, IDC_LBL_FACE_COUNT, m_lblFaceCount);
    DDX_Control(pDX, IDC_LBL_AVG_DIST, m_lblAvgDist);
    DDX_Control(pDX, IDC_LBL_ELAPSED, m_lblElapsed);
    DDX_Control(pDX, IDC_LBL_STATUS, m_lblStatus);
    DDX_Control(pDX, IDC_LBL_CLUSTER_COUNT, m_lblClusterCount);    // ★ 추가
    DDX_Control(pDX, IDC_LBL_CROWDED_CLUSTERS, m_lblCrowdedClusters); // ★ 추가
    DDX_Control(pDX, IDC_LIST_LOG, m_listLog);
}

// ─────────────────────────────────────────────────────────────────────────────
// OnInitDialog
// ─────────────────────────────────────────────────────────────────────────────
BOOL CdensitydetectDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);
    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != nullptr)
    {
        CString strAboutMenu;
        if (strAboutMenu.LoadString(IDS_ABOUTBOX) && !strAboutMenu.IsEmpty())
        {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }
    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    AfxSocketInit();
    m_socket.m_pDlg = this;

    // 컨트롤 초기값
    m_editServer.SetWindowText(_T("127.0.0.1"));
    m_editPort.SetWindowText(_T("7777"));
    m_btnDisconnect.EnableWindow(FALSE);
    m_lblConnStatus.SetWindowText(_T("● 미연결"));
    m_lblFaceCount.SetWindowText(_T("-"));
    m_lblAvgDist.SetWindowText(_T("-"));
    m_lblElapsed.SetWindowText(_T("-"));
    m_lblStatus.SetWindowText(_T("대기 중"));
    m_lblClusterCount.SetWindowText(_T("-"));    // ★ 추가
    m_lblCrowdedClusters.SetWindowText(_T("-"));   // ★ 추가

    m_statusBrush.CreateSolidBrush(m_statusColor);

    AddLog(_T("프로그램 시작됨. 서버에 연결하세요."));
    return TRUE;
}

// ─────────────────────────────────────────────────────────────────────────────
// 연결 버튼
// ─────────────────────────────────────────────────────────────────────────────
void CdensitydetectDlg::OnBtnConnect()
{
    if (m_socket.m_hSocket != INVALID_SOCKET)
        m_socket.Close();
    m_socket.m_recvBuf.Empty();

    CString server, portStr;
    m_editServer.GetWindowText(server);
    m_editPort.GetWindowText(portStr);
    int port = _ttoi(portStr);

    if (!m_socket.Create())
    {
        AfxMessageBox(_T("소켓 생성 실패!"));
        return;
    }

    if (!m_socket.Connect(server, port))
    {
        if (GetLastError() != WSAEWOULDBLOCK)
        {
            AfxMessageBox(_T("서버에 연결할 수 없습니다.\nPython main.py가 실행 중인지 확인하세요."));
            m_socket.Close();
            return;
        }
    }

    AddLog(_T("서버 연결 시도 중..."));
}

// ─────────────────────────────────────────────────────────────────────────────
// 해제 버튼
// ─────────────────────────────────────────────────────────────────────────────
void CdensitydetectDlg::OnBtnDisconnect()
{
    m_socket.Close();
    m_socket.m_recvBuf.Empty();
    m_bConnected = false;

    m_btnConnect.EnableWindow(TRUE);
    m_btnDisconnect.EnableWindow(FALSE);
    m_lblConnStatus.SetWindowText(_T("● 미연결"));
    m_lblStatus.SetWindowText(_T("대기 중"));

    SetStatusColor(_T("PENDING"));
    AddLog(_T("서버 연결 해제됨."));
}

// ─────────────────────────────────────────────────────────────────────────────
// WM_SOCKET_CONNECTED – 연결 성공/실패
// ─────────────────────────────────────────────────────────────────────────────
LRESULT CdensitydetectDlg::OnSocketConnected(WPARAM wParam, LPARAM)
{
    if (wParam == 0)  // 연결 성공
    {
        m_bConnected = true;
        m_btnConnect.EnableWindow(FALSE);
        m_btnDisconnect.EnableWindow(TRUE);
        m_lblConnStatus.SetWindowText(_T("● 연결됨"));
        AddLog(_T("서버 연결 성공!"));
    }
    else  // 연결 실패
    {
        m_socket.Close();
        m_btnConnect.EnableWindow(TRUE);
        m_btnDisconnect.EnableWindow(FALSE);
        m_lblConnStatus.SetWindowText(_T("● 미연결"));
        SetStatusColor(_T("PENDING"));
        AddLog(_T("서버 연결 실패. Python이 실행 중인지 확인하세요."));
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// WM_SOCKET_RECEIVED – JSON 한 줄 수신
// ─────────────────────────────────────────────────────────────────────────────
LRESULT CdensitydetectDlg::OnSocketReceived(WPARAM /*wParam*/, LPARAM lParam)
{
    CString* pLine = reinterpret_cast<CString*>(lParam);
    if (!pLine) return 0;

    CString json = *pLine;
    delete pLine;

    // ── JSON 파싱 ────────────────────────────────────────────────────────────
    CString fcStr = ParseJsonField(json, _T("face_count"));
    CString distStr = ParseJsonField(json, _T("avg_distance"));
    CString elStr = ParseJsonField(json, _T("elapsed"));
    CString clStr = ParseJsonField(json, _T("cluster_count"));    // ★ 추가
    CString ccStr = ParseJsonField(json, _T("crowded_clusters")); // ★ 추가
    CString status = ParseJsonField(json, _T("status"));

    int    faceCount = _ttoi(fcStr);
    double avgDist = _tcstod(distStr, nullptr);
    double elapsed = _tcstod(elStr, nullptr);
    int    clusterCount = _ttoi(clStr);    // ★ 추가
    int    crowdedClusters = _ttoi(ccStr);    // ★ 추가

    UpdateUI(faceCount, avgDist, elapsed, clusterCount, crowdedClusters, status);
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// WM_SOCKET_CLOSED – 서버 연결 끊김
// ─────────────────────────────────────────────────────────────────────────────
LRESULT CdensitydetectDlg::OnSocketClosed(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    m_bConnected = false;
    m_btnConnect.EnableWindow(TRUE);
    m_btnDisconnect.EnableWindow(FALSE);
    m_lblConnStatus.SetWindowText(_T("● 미연결"));
    SetStatusColor(_T("PENDING"));
    AddLog(_T("서버 연결이 끊어졌습니다."));
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// UI 갱신
// ─────────────────────────────────────────────────────────────────────────────
void CdensitydetectDlg::UpdateUI(int faceCount, double avgDist, double elapsed,
    int clusterCount, int crowdedClusters,
    const CString& status)
{
    CString tmp;

    // 탐지 인원
    tmp.Format(_T("%d 명"), faceCount);
    m_lblFaceCount.SetWindowText(tmp);

    // 평균 거리: -1이면 측정 불가
    if (avgDist < 0)
        m_lblAvgDist.SetWindowText(_T("측정 불가 (3명 미만)"));
    else
    {
        tmp.Format(_T("%.2f"), avgDist);
        m_lblAvgDist.SetWindowText(tmp);
    }

    // 혼잡 지속 시간
    tmp.Format(_T("%.1f 초"), elapsed);
    m_lblElapsed.SetWindowText(tmp);

    // 혼잡도 텍스트
    m_lblStatus.SetWindowText(status);

    // ★ 추가: 전체 클러스터 수
    tmp.Format(_T("%d 개"), clusterCount);
    m_lblClusterCount.SetWindowText(tmp);

    // ★ 추가: 혼잡 클러스터 수  (예: "1 / 2 개" 형태로 직관적으로 표시)
    tmp.Format(_T("%d / %d 개"), crowdedClusters, clusterCount);
    m_lblCrowdedClusters.SetWindowText(tmp);

    // 색상 패널
    SetStatusColor(status);

    // 로그
    CString log;
    if (avgDist < 0)
        log.Format(_T("인원:%d | 거리:- | 경과:%.1fs | 클러스터:%d(혼잡:%d) | %s"),
            faceCount, elapsed, clusterCount, crowdedClusters, (LPCTSTR)status);
    else
        log.Format(_T("인원:%d | 거리:%.2f | 경과:%.1fs | 클러스터:%d(혼잡:%d) | %s"),
            faceCount, avgDist, elapsed, clusterCount, crowdedClusters, (LPCTSTR)status);
    AddLog(log);
}

// ─────────────────────────────────────────────────────────────────────────────
// 색상 패널 갱신
// ─────────────────────────────────────────────────────────────────────────────
void CdensitydetectDlg::SetStatusColor(const CString& status)
{
    COLORREF newColor;

    if (status.Find(_T("GREEN")) != -1) newColor = RGB(0, 200, 80);
    else if (status.Find(_T("YELLOW")) != -1) newColor = RGB(255, 210, 0);
    else if (status.Find(_T("ORANGE")) != -1) newColor = RGB(255, 120, 0);
    else if (status.Find(_T("RED")) != -1) newColor = RGB(220, 30, 30);
    else                                       newColor = RGB(160, 160, 160);

    if (newColor != m_statusColor)
    {
        m_statusColor = newColor;
        m_statusBrush.DeleteObject();
        m_statusBrush.CreateSolidBrush(m_statusColor);
        m_lblStatusBox.Invalidate();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// OnCtlColor – 색상 패널 배경색 적용
// ─────────────────────────────────────────────────────────────────────────────
HBRUSH CdensitydetectDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

    if (pWnd->GetDlgCtrlID() == IDC_LBL_STATUS_BOX)
    {
        pDC->SetBkColor(m_statusColor);
        return (HBRUSH)m_statusBrush;
    }
    return hbr;
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON 파싱
// ─────────────────────────────────────────────────────────────────────────────
CString CdensitydetectDlg::ParseJsonField(const CString& json, const CString& key)
{
    CString pattern = _T("\"") + key + _T("\":");
    int pos = json.Find(pattern);
    if (pos == -1) return _T("");

    pos += pattern.GetLength();
    while (pos < json.GetLength() && json[pos] == _T(' ')) pos++;

    bool inString = (pos < json.GetLength() && json[pos] == _T('"'));
    if (inString) pos++;

    CString value;
    for (; pos < json.GetLength(); pos++)
    {
        TCHAR c = json[pos];
        if (inString && c == _T('"'))                      break;
        if (!inString && (c == _T(',') || c == _T('}')))   break;
        value += c;
    }
    return value.Trim();
}

// ─────────────────────────────────────────────────────────────────────────────
// 로그 추가
// ─────────────────────────────────────────────────────────────────────────────
void CdensitydetectDlg::AddLog(const CString& msg)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    CString entry;
    entry.Format(_T("[%02d:%02d:%02d] %s"),
        st.wHour, st.wMinute, st.wSecond, (LPCTSTR)msg);

    m_listLog.InsertString(0, entry);

    while (m_listLog.GetCount() > 200)
        m_listLog.DeleteString(m_listLog.GetCount() - 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// 종료 시 소켓 정리
// ─────────────────────────────────────────────────────────────────────────────
void CdensitydetectDlg::OnDestroy()
{
    if (m_socket.m_hSocket != INVALID_SOCKET)
        m_socket.Close();
    CDialogEx::OnDestroy();
}

// ─────────────────────────────────────────────────────────────────────────────
// 기존 코드 (그대로 유지)
// ─────────────────────────────────────────────────────────────────────────────
void CdensitydetectDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX)
    {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    }
    else
    {
        CDialogEx::OnSysCommand(nID, lParam);
    }
}

void CdensitydetectDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this);
        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        dc.DrawIcon((rect.Width() - cxIcon + 1) / 2, (rect.Height() - cyIcon + 1) / 2, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();
    }
}

HCURSOR CdensitydetectDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}