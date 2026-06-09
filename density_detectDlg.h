#pragma once
#include "resource.h"
#include <afxsock.h>

// ─────────────────────────────────────────────────────────────────────────────
// 커스텀 메시지
// ─────────────────────────────────────────────────────────────────────────────
#define WM_SOCKET_RECEIVED  (WM_USER + 1)
#define WM_SOCKET_CLOSED    (WM_USER + 2)
#define WM_SOCKET_CONNECTED (WM_USER + 3)

class CdensitydetectDlg;

// ─────────────────────────────────────────────────────────────────────────────
// CClientSocket
// ─────────────────────────────────────────────────────────────────────────────
class CClientSocket : public CAsyncSocket
{
public:
    CdensitydetectDlg* m_pDlg = nullptr;
    CString            m_recvBuf;

    void OnReceive(int nErrorCode) override;
    void OnClose(int nErrorCode) override;
    void OnConnect(int nErrorCode) override;
};

// ─────────────────────────────────────────────────────────────────────────────
// CdensitydetectDlg
// ─────────────────────────────────────────────────────────────────────────────
class CdensitydetectDlg : public CDialogEx
{
public:
    CdensitydetectDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DENSITY_DETECT_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    // ── 컨트롤 멤버 변수 ────────────────────────────────────────────────────
    CEdit    m_editServer;
    CEdit    m_editPort;
    CButton  m_btnConnect;
    CButton  m_btnDisconnect;
    CStatic  m_lblConnStatus;
    CStatic  m_lblStatusBox;
    CStatic  m_lblFaceCount;
    CStatic  m_lblAvgDist;
    CStatic  m_lblElapsed;
    CStatic  m_lblStatus;
    CStatic  m_lblClusterCount;      // ★ 추가: 전체 클러스터 수
    CStatic  m_lblCrowdedClusters;   // ★ 추가: 혼잡 클러스터 수
    CListBox m_listLog;

    // ── 소켓 ────────────────────────────────────────────────────────────────
    CClientSocket m_socket;
    bool          m_bConnected = false;

    // ── 색상 ────────────────────────────────────────────────────────────────
    COLORREF m_statusColor = RGB(160, 160, 160);
    CBrush   m_statusBrush;

    // ── 헬퍼 ────────────────────────────────────────────────────────────────
    void    AddLog(const CString& msg);
    void    UpdateUI(int faceCount, double avgDist, double elapsed,
        int clusterCount, int crowdedClusters,   // ★ 추가
        const CString& status);
    void    SetStatusColor(const CString& status);
    CString ParseJsonField(const CString& json, const CString& key);

    // ── 메시지 핸들러 ───────────────────────────────────────────────────────
    afx_msg void    OnBtnConnect();
    afx_msg void    OnBtnDisconnect();
    afx_msg void    OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void    OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void    OnDestroy();
    afx_msg HBRUSH  OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg LRESULT OnSocketReceived(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSocketClosed(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSocketConnected(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    HICON m_hIcon;
};