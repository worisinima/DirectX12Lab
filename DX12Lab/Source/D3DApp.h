#pragma once

#include "Renderer.h"

class D3DApp
{
public:

	D3DApp(HINSTANCE hInstance);
	D3DApp(const D3DApp& rhs) = delete;
	D3DApp& operator=(const D3DApp& rhs) = delete;
	virtual ~D3DApp();

	static D3DApp* GetApp();

	HINSTANCE AppInst() const;
	HWND      MainWnd() const;

	bool Get4xMsaaState()const;
	void Set4xMsaaState(bool value);

	int Run();

	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	static D3DApp* mApp;

	bool Initialize();
	virtual void Update(const GameTimer& gt);

	bool InitMainWindow();

	void CalculateFrameStats();

	HINSTANCE mhAppInst = nullptr; // application instance handle
	HWND      mhMainWnd = nullptr; // main window handle
	bool      mAppPaused = false;  // is the application paused?
	bool      mMinimized = false;  // is the application minimized?
	bool      mMaximized = false;  // is the application maximized?
	bool      mResizing = false;   // are the resize bars being dragged?
	bool      mFullscreenState = false;// fullscreen enabled
	// Set true to use 4X MSAA (?.1.8).  The default is false.
	bool      m4xMsaaState = false;    // 4X MSAA enabled
	UINT      m4xMsaaQuality = 0;      // quality level of 4X MSAA

	int GetClientWidth(){ return mClientWidth; }
	int GetClientHeight(){ return mClientHeight; }
private:

	//Renderer of this app
	Renderer* GraphicRender;

	GameTimer mTimer;
	WNDCLASSEX wc;
	// Derived class should set these in derived constructor to customize starting values.
	std::wstring mMainWndCaption = L"DirectX12Lab";
	int mClientWidth = 1920;
	int mClientHeight = 1080;

	// Convenience overrides for handling mouse input.
	virtual void OnMouseDown(WPARAM btnState, int x, int y);
	virtual void OnMouseUp(WPARAM btnState, int x, int y);
	virtual void OnMouseMove(WPARAM btnState, int x, int y);
	void OnKeyboardInput(const GameTimer& gt);

	POINT mLastMousePos;

private:
	
	void BeginRenderUI();
	void EndRenderUI();
	void DrawUIContent();

};
