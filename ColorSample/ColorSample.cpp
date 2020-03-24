#ifndef STRICT
#define STRICT	// 厳密なコードを型を要求する
#endif
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <k4a/k4a.h>

#pragma comment( lib, "k4a.lib" )

#define ENABLE_CSV_OUTPUT		1			// 1=CSV 出力を有効にする

// イメージの解像度
#define RESOLUTION_WIDTH		(1280)
#define RESOLUTION_HEIGHT		(720)

// Win32 アプリ用のパラメータ
static const TCHAR szClassName[] = TEXT("カラーサンプル");
HWND g_hWnd = NULL;							// アプリケーションのウィンドウ
HBITMAP g_hBMP = NULL, g_hBMPold = NULL;	// 表示するビットマップのハンドル
HDC g_hDCBMP = NULL;						// 表示するビットマップのコンテキスト
BITMAPINFO g_biBMP = { 0, };				// ビットマップの情報 (解像度やフォーマット)
LPDWORD g_pdwPixel = NULL;					// ビットマップの中身の先頭 (ピクセル情報)

// Azure Kinect 用のパラメータ
k4a_device_t g_hAzureKinect = nullptr;		// Azure Kinect のデバイスハンドル

// Kinect を初期化する
k4a_result_t CreateKinect()
{
	k4a_result_t hr;

	// Azure Kinect を初期化する
	hr = k4a_device_open( K4A_DEVICE_DEFAULT, &g_hAzureKinect );
	if ( hr == K4A_RESULT_SUCCEEDED )
	{
		// Azure Kinect のカメラ設定
		k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
		config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
		config.color_resolution = K4A_COLOR_RESOLUTION_720P;
		config.depth_mode = K4A_DEPTH_MODE_OFF;
		config.camera_fps = K4A_FRAMES_PER_SECOND_30;
		config.synchronized_images_only = false;
		config.depth_delay_off_color_usec = 0;
		config.wired_sync_mode = K4A_WIRED_SYNC_MODE_STANDALONE;
		config.subordinate_delay_off_master_usec = 0;
		config.disable_streaming_indicator = false;

		// Azure Kinect の使用を開始する
		hr = k4a_device_start_cameras( g_hAzureKinect, &config );
		if ( hr == K4A_RESULT_SUCCEEDED )
		{
			return hr;
		}
		else
		{
			MessageBox( NULL, TEXT("Azure Kinect が開始できませんでした"), TEXT("エラー"), MB_OK );
		}
		// Azure Kinect の使用をやめる
		k4a_device_close( g_hAzureKinect );
	}
	else
	{
		MessageBox( NULL, TEXT("Azure Kinect の初期化に失敗 - カメラの状態を確認してください"), TEXT("エラー"), MB_OK );
	}
	return hr;
}

// Kinect を終了する
void DestroyKinect()
{
	if ( g_hAzureKinect )
	{
		// Azure Kinect を停止する
		k4a_device_stop_cameras( g_hAzureKinect );

		// Azure Kinect の使用をやめる
		k4a_device_close( g_hAzureKinect );
		g_hAzureKinect = nullptr;
	}
}

// Kinect のメインループ処理
uint32_t KinectProc()
{
	k4a_wait_result_t hr;
	uint32_t uImageSize = 0;

	// キャプチャーする
	k4a_capture_t hCapture = nullptr;
	hr = k4a_device_get_capture( g_hAzureKinect, &hCapture, K4A_WAIT_INFINITE );
	if ( hr == K4A_WAIT_RESULT_SUCCEEDED )
	{
		k4a_image_t hImage;

		// カラーイメージを取得する
		hImage = k4a_capture_get_color_image( hCapture );

		// キャプチャーを開放する
		k4a_capture_release( hCapture );

		if ( hImage )
		{
			// イメージピクセルの先頭ポインタを取得する
			uint8_t* p = k4a_image_get_buffer( hImage );
			if ( p )
			{
				// イメージサイズを取得する
				uImageSize = (uint32_t) k4a_image_get_size( hImage );
				CopyMemory( g_pdwPixel, p, uImageSize );
			}
			// イメージを開放する
			k4a_image_release( hImage );
		}
	}
	return uImageSize;
}

#if ENABLE_CSV_OUTPUT
// CSV ファイルにデータを出力する
void WriteCSV()
{
	HANDLE hFile = CreateFileA( "color.csv", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE )
	{
		for( int y = 0; y < RESOLUTION_HEIGHT; y++ )
		{
			// カラー情報を CSV に出力
			char szText[8192] = "";
			for( int x = 0; x < RESOLUTION_WIDTH; x++ )
			{
				const DWORD dw = g_pdwPixel[y * RESOLUTION_WIDTH + x];
				const BYTE c = ((dw & 0xFF) + ((dw >> 8) & 0xFF) + ((dw >> 16) & 0xFF)) / 3;
				sprintf_s( szText, 8192, "%s%d,", szText, c );
			}

			// 改行してファイル出力
			DWORD dwWritten;
			DWORD dwLen = (DWORD) strlen( szText );
			strcpy_s( &szText[dwLen - 1], 8192, "\r\n" );
			WriteFile( hFile, szText, dwLen, &dwWritten, NULL );
		}
		CloseHandle( hFile );
	}
}
#endif

LRESULT CALLBACK WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch( uMsg )
	{
	case WM_PAINT:
		{
			// 画面表示処理
			PAINTSTRUCT ps;
			HDC hDC = BeginPaint( hWnd, &ps );

			// 画面サイズを取得する
			RECT rect;
			GetClientRect( hWnd, &rect );

			// カラーの表示
			StretchBlt( hDC, 0, 0, rect.right, rect.bottom, g_hDCBMP, 0, 0, RESOLUTION_WIDTH, RESOLUTION_HEIGHT, SRCCOPY );
			EndPaint( hWnd, &ps );
		}
		return 0;
#if ENABLE_CSV_OUTPUT
	case WM_KEYDOWN:
		// スペースキーが押されたら CSV 出力する
		if ( wParam == VK_SPACE )
			WriteCSV();
		break;
#endif
	case WM_CLOSE:
		DestroyWindow( hWnd );
	case WM_DESTROY:
		PostQuitMessage( 0 );
		break;
	default:
		return DefWindowProc( hWnd, uMsg, wParam, lParam );
	}
	return 0;
}

// アプリケーションの初期化 (ウィンドウや描画用のビットマップを作成)
HRESULT InitApp( HINSTANCE hInst, int nCmdShow )
{
	WNDCLASSEX wc = { 0, };
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH) GetStockObject( NULL_BRUSH );
	wc.lpszClassName = szClassName;
	wc.hIconSm = LoadIcon( NULL, IDI_APPLICATION );
	if ( ! RegisterClassEx( &wc ) )
	{
		MessageBox( NULL, TEXT("アプリケーションクラスの初期化に失敗"), TEXT("エラー"), MB_OK );
		return E_FAIL;
	}

	// アプリケーションウィンドウを作成する
	g_hWnd = CreateWindow( szClassName, szClassName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInst, NULL );
	if ( ! g_hWnd )
	{
		MessageBox( NULL, TEXT("ウィンドウの初期化に失敗"), TEXT("エラー"), MB_OK );
		return E_FAIL;
	}

	// 画面表示用のビットマップを作成する
	ZeroMemory( &g_biBMP, sizeof(g_biBMP) );
	g_biBMP.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	g_biBMP.bmiHeader.biBitCount = 32;
	g_biBMP.bmiHeader.biPlanes = 1;
	g_biBMP.bmiHeader.biWidth = RESOLUTION_WIDTH;
	g_biBMP.bmiHeader.biHeight = -(int) RESOLUTION_HEIGHT;
	g_hBMP = CreateDIBSection( NULL, &g_biBMP, DIB_RGB_COLORS, (LPVOID*) (&g_pdwPixel), NULL, 0 );
	HDC hDC = GetDC( g_hWnd );
	g_hDCBMP = CreateCompatibleDC( hDC );
	ReleaseDC( g_hWnd, hDC );
	g_hBMPold = (HBITMAP) SelectObject( g_hDCBMP, g_hBMP );

	ShowWindow( g_hWnd, nCmdShow );
	UpdateWindow( g_hWnd );

	return S_OK;
}

// アプリケーションの後始末
HRESULT UninitApp()
{
	// 画面表示用のビットマップを開放する
	if ( g_hDCBMP || g_hBMP )
	{
		SelectObject( g_hDCBMP, g_hBMPold );
		DeleteObject( g_hBMP );
		DeleteDC( g_hDCBMP );
		g_hBMP = NULL;
		g_hDCBMP = NULL;
	}
	return S_OK;
}

// エントリーポイント
int WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow )
{
	// アプリケーションの初期化関数を呼ぶ
	if ( FAILED( InitApp( hInst, nCmdShow ) ) )
		return 1;

	// Kinect の初期化関数を呼ぶ
	if ( FAILED( CreateKinect() ) )
		return 1;

	// アプリケーションループ
	MSG msg;
	while( GetMessage( &msg, NULL, 0, 0 ) )
	{
		// ウィンドウメッセージを処理
		TranslateMessage( &msg );
		DispatchMessage( &msg );

		// Kinect 処理関数を呼ぶ
		if ( KinectProc() )
		{
			// Kinect 情報に更新があれば描画メッセージを発行する
			InvalidateRect( g_hWnd, NULL, TRUE );
		}
	}

	// Kinect の終了関数を呼ぶ
	DestroyKinect();

	// アプリケーションを終了関数を呼ぶ
	UninitApp();

	return (int) msg.wParam;
}
