//d3d11 hook

#include <Windows.h>
#include <string>
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

//detours
#include "detoursX64/detours.h"
#pragma comment(lib, "detoursX64/detours.lib")

//==========================================================================================================================//

typedef void(__stdcall *D3D11DRAWINDEXED) (ID3D11DeviceContext *pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
D3D11DRAWINDEXED orig_D3D11DrawIndexed = NULL;

typedef void(__stdcall* D3D11DRAWINDEXEDINSTANCED) (ID3D11DeviceContext* pContext, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation);
D3D11DRAWINDEXEDINSTANCED orig_D3D11DrawIndexedInstanced = NULL;

//==========================================================================================================================//

void **contextvtable = NULL;
void **devicevtable = NULL;

ID3D11Device *pDevice = NULL;
ID3D11DeviceContext *pContext = NULL;

//==========================================================================================================================//

//init only once
bool firstTime = true;

//shader
ID3D11PixelShader* xsGreen = NULL;
ID3D11PixelShader* xsYellow = NULL;
ID3D11PixelShader* xsRed = NULL;
ID3D11PixelShader* xsMagenta = NULL;
ID3D11PixelShader* xsBlue = NULL;
ID3D11PixelShader* xsWhite = NULL;
ID3D11PixelShader* xsGray = NULL;
ID3D11PixelShader* xsAntiDark = NULL;

ID3D11SamplerState* sampleYellow = NULL;

#define SAFE_RELEASE(x) if (x) { x->Release(); x = NULL; }
HRESULT hr;
bool toggler = false;
int countnum = -1;

//==========================================================================================================================//

using namespace std;
#include <fstream>

//log
void Log(const char *fmt, ...)
{
	if (!fmt)	return;

	char		text[4096];
	va_list		ap;
	va_start(ap, fmt);
	vsprintf_s(text, fmt, ap);
	va_end(ap);

	ofstream logfile((PCHAR)"log.txt", ios::app);
	if (logfile.is_open() && text)	logfile << text << endl;
	logfile.close();
}


#include <D3Dcompiler.h> //generateshader
#pragma comment(lib, "D3dcompiler.lib")
//generateshader
HRESULT cryptspShader(ID3D11Device* pDevice, ID3D11PixelShader** pShader, float r, float g, float b)
{
	char szCast[] = "struct VS_OUT"
		"{"
		" float4 Position : SV_Position;"
		" float4 Color : COLOR0;"
		"};"

		"float4 main( VS_OUT input ) : SV_Target"
		"{"
		" float4 col;"
		" col.a = 1.0f;"
		" col.r = %f;"
		" col.g = %f;"
		" col.b = %f;"
		" return col;"
		"}";
	
	ID3D10Blob* pBlob;
	char szPixelShader[1000];

	sprintf_s(szPixelShader, szCast, r, g, b);

	ID3DBlob* error;

	HRESULT hr = D3DCompile(szPixelShader, sizeof(szPixelShader), "shader", NULL, NULL, "main", "ps_4_0", NULL, NULL, &pBlob, &error);

	if (FAILED(hr))
		return hr;

	hr = pDevice->CreatePixelShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, pShader);

	if (FAILED(hr))
		return hr;

	return S_OK;
}

//==========================================================================================================================

void __stdcall hook_D3D11DrawIndexed(ID3D11DeviceContext *pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	if (pContext == nullptr) return orig_D3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);

	if (firstTime)
	{
		firstTime = false; //only once

		//get device
		pContext->GetDevice(&pDevice);

		/*
		//createsamplerstate
		D3D11_SAMPLER_DESC samplerDesc;
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerDesc.BorderColor[0] = 1.0f;
		samplerDesc.BorderColor[1] = 1.0f;
		samplerDesc.BorderColor[2] = 0.0f;
		samplerDesc.BorderColor[3] = 1.0f;
		pDevice->CreateSamplerState(&samplerDesc, &sampleYellow);
		*/
	}

	//create shaders
	if (!xsGreen)
		cryptspShader(pDevice, &xsGreen, 0.0f, 1.0f, 0.0f); //valuable item light

	if (!xsYellow)
		cryptspShader(pDevice, &xsYellow, 1.0f, 1.0f, 0.0f); //valuable item

	if (!xsMagenta)
		cryptspShader(pDevice, &xsMagenta, 1.0f, 0.0f, 1.0f); //azurite ore

	if (!xsRed)
		cryptspShader(pDevice, &xsRed, 1.0f, 0.0f, 0.0f); //azurite ore 2

	if (!xsBlue)
		cryptspShader(pDevice, &xsBlue, 0.0f, 0.0f, 0.5f); //chests

	if (!xsWhite)
		cryptspShader(pDevice, &xsWhite, 0.7f, 0.7f, 0.7f); //forgotten armour

	if (!xsGray)
		cryptspShader(pDevice, &xsGray, 0.2f, 0.2f, 0.2f); //lost armaments

	if (!xsAntiDark)
		cryptspShader(pDevice, &xsAntiDark, 0.01f, 0.02f, 0.01f); //infra green

	ID3D11Buffer *veBuffer;
	UINT veWidth;
	UINT Stride;
	UINT veBufferOffset;
	D3D11_BUFFER_DESC veDesc;

	ID3D11Buffer *inBuffer;
	UINT inWidth;
	DXGI_FORMAT inFormat;
	UINT inOffset;
	D3D11_BUFFER_DESC inDesc;

	pContext->IAGetVertexBuffers(0, 1, &veBuffer, &Stride, &veBufferOffset);
	if (veBuffer) {
		veBuffer->GetDesc(&veDesc);
		veWidth = veDesc.ByteWidth;
	}
	if (NULL != veBuffer) {
		veBuffer->Release();
		veBuffer = NULL;
	}

	pContext->IAGetIndexBuffer(&inBuffer, &inFormat, &inOffset);
	if (inBuffer) {
		inBuffer->GetDesc(&inDesc);
		inWidth = inDesc.ByteWidth;
	}
	if (NULL != inBuffer) {
		inBuffer->Release();
		inBuffer = NULL;
	}

	//model rec
	if (Stride == 24 && IndexCount == 192 && inWidth == 1536 && veWidth == 6528)			//valuable item light
	{
		pContext->PSSetShader(xsGreen, NULL, NULL);
	}

	if(Stride == 32 && IndexCount == 54600 && inWidth == 109200 && veWidth == 563040)		//fossil
	{
		//pContext->PSSetSamplers(0, 1, &sampleYellow);
		pContext->PSSetShader(xsYellow, NULL, NULL);
	}

	if (Stride == 32 && IndexCount == 31086 && inWidth == 62172 && veWidth == 335744)		//flawed azurite vein 85
	{
		pContext->PSSetShader(xsRed, NULL, NULL);
	}

	if (Stride == 32 && IndexCount == 30144 && inWidth == 60288 && veWidth == 314624)		//azurite vein 124
	{
		pContext->PSSetShader(xsRed, NULL, NULL);
	}

	if (Stride == 32 && IndexCount == 27516 && inWidth == 55032 && veWidth == 299744)		//rich azurite vein 186
	{
		pContext->PSSetShader(xsRed, NULL, NULL);
	}

	if(Stride == 32 && IndexCount == 29184 && inWidth == 58368 && veWidth == 316384)		//pure azurite vein 296	
	{
		pContext->PSSetShader(xsMagenta, NULL, NULL);
	}

	if ((Stride == 32 && IndexCount == 15774 && inWidth == 31572 && veWidth == 233376)	||	//wall
		//(Stride == 32 && IndexCount == 3102 && inWidth == 6204 && veWidth == 31552)		||	//chest, resonator chest Stride == 32 && IndexCount == 3120 && inWidth == 9144 && veWidth == 40896
		//(Stride == 32 && IndexCount == 2577 && inWidth == 5706 && veWidth == 39552)		||	//flare chest
		(Stride == 32 && IndexCount == 1812 && inWidth == 3624 && veWidth == 25312))		//ring chest
	{
		pContext->PSSetShader(xsBlue, NULL, NULL);
		//pContext->PSSetShader(0, NULL, NULL);
	}

	if(Stride == 32 && IndexCount == 3102 && inWidth == 6204 && veWidth == 31552)
	{
		pContext->PSSetShader(xsMagenta, NULL, NULL);
	}

	if (Stride == 32 && IndexCount == 2577 && inWidth == 5706 && veWidth == 39552)			//dynamite & flare chest
	{
		pContext->PSSetShader(0, NULL, NULL);
	}

	if(Stride == 32 && IndexCount == 10272 && inWidth == 20544 && veWidth == 93952)			//forgotten armour
	{
		pContext->PSSetShader(xsWhite, NULL, NULL);
	}

	if(Stride == 32 && IndexCount == 2622 && inWidth == 10644 && veWidth == 76640)			//lost armaments
	{
		pContext->PSSetShader(xsGray, NULL, NULL);
	}

	//if(Stride == 32 && IndexCount == 22176 && inWidth == 44376 && veWidth == 360224||
	//Stride == 32 && IndexCount == 24930 && inWidth == 49884 && veWidth == 407488) //destroyable wall
	//{
		//pContext->PSSetShader(0, NULL, NULL);
	//}

	//if (Stride == 32 && IndexCount > 120) //models
	//{
	//}

	//Stride == 20 && IndexCount == 6 && inWidth == 12 && veWidth == 80
	//anti darkness
	if (GetAsyncKeyState(VK_F10) & 1)
		toggler = !toggler;
	if (toggler)
		if (Stride == 20 && veWidth < 3000) //&& Descr.Format == countnum)//28, 76
		{
			pContext->PSSetShader(xsAntiDark, NULL, NULL);
		}

	//log
	//if (Stride == 20 && countnum == veWidth/1)
	if (countnum == IndexCount/100)
		if (GetAsyncKeyState('I') & 1)
			Log("Stride == %d && IndexCount == %d && inWidth == %d && veWidth == %d",
				Stride, IndexCount, inWidth, veWidth);

	//if (Stride == 20 && countnum == veWidth / 1)
	if (countnum == IndexCount / 100)
	{
		//pContext->PSSetShader(xsAntiDark, NULL, NULL);
		return;
	}

	if (GetAsyncKeyState('O') & 1) //-
		countnum--;
	if (GetAsyncKeyState('P') & 1) //+
		countnum++;
	if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState('9') & 1) //reset, set to -1
		countnum = -1;
	
	return orig_D3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}

//==========================================================================================================================//

//this is called for me since new patch
void __stdcall hook_D3D11DrawIndexedInstanced(ID3D11DeviceContext* pContext, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	if (firstTime)
	{
		firstTime = false; //only once

		//get device
		pContext->GetDevice(&pDevice);

		/*
		//createsamplerstate
		D3D11_SAMPLER_DESC samplerDesc;
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		samplerDesc.BorderColor[0] = 1.0f;
		samplerDesc.BorderColor[1] = 1.0f;
		samplerDesc.BorderColor[2] = 0.0f;
		samplerDesc.BorderColor[3] = 1.0f;
		pDevice->CreateSamplerState(&samplerDesc, &sampleYellow);
		*/
	}

	//create shaders
	if (!xsGreen)
		cryptspShader(pDevice, &xsGreen, 0.0f, 1.0f, 0.0f); //valuable item light

	if (!xsYellow)
		cryptspShader(pDevice, &xsYellow, 1.0f, 1.0f, 0.0f); //valuable item

	if (!xsMagenta)
		cryptspShader(pDevice, &xsMagenta, 1.0f, 0.0f, 1.0f); //azurite ore

	if (!xsRed)
		cryptspShader(pDevice, &xsRed, 1.0f, 0.0f, 0.0f); //azurite ore 2

	if (!xsBlue)
		cryptspShader(pDevice, &xsBlue, 0.0f, 0.0f, 0.5f); //chests

	if (!xsWhite)
		cryptspShader(pDevice, &xsWhite, 0.7f, 0.7f, 0.7f); //forgotten armour

	if (!xsGray)
		cryptspShader(pDevice, &xsGray, 0.2f, 0.2f, 0.2f); //lost armaments

	if (!xsAntiDark)
		cryptspShader(pDevice, &xsAntiDark, 0.01f, 0.02f, 0.01f); //infra green

	ID3D11Buffer* veBuffer;
	UINT veWidth;
	UINT Stride;
	UINT veBufferOffset;
	D3D11_BUFFER_DESC veDesc;

	ID3D11Buffer* inBuffer;
	UINT inWidth;
	DXGI_FORMAT inFormat;
	UINT inOffset;
	D3D11_BUFFER_DESC inDesc;

	pContext->IAGetVertexBuffers(0, 1, &veBuffer, &Stride, &veBufferOffset);
	if (veBuffer) {
		veBuffer->GetDesc(&veDesc);
		veWidth = veDesc.ByteWidth;
	}
	if (NULL != veBuffer) {
		veBuffer->Release();
		veBuffer = NULL;
	}

	pContext->IAGetIndexBuffer(&inBuffer, &inFormat, &inOffset);
	if (inBuffer) {
		inBuffer->GetDesc(&inDesc);
		inWidth = inDesc.ByteWidth;
	}
	if (NULL != inBuffer) {
		inBuffer->Release();
		inBuffer = NULL;
	}

	//model rec
	if (Stride == 24 && IndexCountPerInstance == 192 && inWidth == 1536 && veWidth == 6528)				//valuable item light
	{
		pContext->PSSetShader(xsGreen, NULL, NULL);
	}

	if (Stride == 32 && IndexCountPerInstance == 54600 && inWidth == 109200 && veWidth == 563040)		//fossil
	{
		//pContext->PSSetSamplers(0, 1, &sampleYellow);
		pContext->PSSetShader(xsYellow, NULL, NULL);
	}

	if (Stride == 32 && IndexCountPerInstance == 31086 && inWidth == 62172 && veWidth == 335744)		//flawed azurite vein 85
	{
		pContext->PSSetShader(xsRed, NULL, NULL);
	}

	if (Stride == 32 && IndexCountPerInstance == 30144 && inWidth == 60288 && veWidth == 314624)		//azurite vein 124
	{
		pContext->PSSetShader(xsRed, NULL, NULL);
	}

	if (Stride == 32 && IndexCountPerInstance == 27516 && inWidth == 55032 && veWidth == 299744)		//rich azurite vein 186
	{
		pContext->PSSetShader(xsRed, NULL, NULL);
	}

	if (Stride == 32 && IndexCountPerInstance == 29184 && inWidth == 58368 && veWidth == 316384)		//pure azurite vein 296	
	{
		pContext->PSSetShader(xsMagenta, NULL, NULL);
	}

	if ((Stride == 32 && IndexCountPerInstance == 15774 && inWidth == 31572 && veWidth == 233376) ||	//wall
		//(Stride == 32 && IndexCountPerInstance == 3102 && inWidth == 6204 && veWidth == 31552)		||	//chest, resonator chest Stride == 32 && IndexCountPerInstance == 3120 && inWidth == 9144 && veWidth == 40896
		//(Stride == 32 && IndexCountPerInstance == 2577 && inWidth == 5706 && veWidth == 39552)		||	//flare chest
		(Stride == 32 && IndexCountPerInstance == 1812 && inWidth == 3624 && veWidth == 25312))		//ring chest
	{
		pContext->PSSetShader(xsBlue, NULL, NULL);
		//pContext->PSSetShader(0, NULL, NULL);
	}

	if (Stride == 32 && IndexCountPerInstance == 3102 && inWidth == 6204 && veWidth == 31552)
	{
		pContext->PSSetShader(xsMagenta, NULL, NULL);
	}

	if (Stride == 32 && IndexCountPerInstance == 2577 && inWidth == 5706 && veWidth == 39552)			//dynamite & flare chest
	{
		pContext->PSSetShader(0, NULL, NULL);
	}

	if (Stride == 32 && IndexCountPerInstance == 10272 && inWidth == 20544 && veWidth == 93952)			//forgotten armour
	{
		pContext->PSSetShader(xsWhite, NULL, NULL);
	}

	if (Stride == 32 && IndexCountPerInstance == 2622 && inWidth == 10644 && veWidth == 76640)			//lost armaments
	{
		pContext->PSSetShader(xsGray, NULL, NULL);
	}

	//if(Stride == 32 && IndexCountPerInstance == 22176 && inWidth == 44376 && veWidth == 360224||
	//Stride == 32 && IndexCountPerInstance == 24930 && inWidth == 49884 && veWidth == 407488) //destroyable wall
	//{
		//pContext->PSSetShader(0, NULL, NULL);
	//}

	//if (Stride == 32 && IndexCountPerInstance > 120) //models
	//{
	//}

	//Stride == 20 && IndexCountPerInstance == 6 && inWidth == 12 && veWidth == 80
	//anti darkness
	if (GetAsyncKeyState(VK_F10) & 1)
		toggler = !toggler;
	if (toggler)
		if (Stride == 20 && veWidth < 3000) //&& Descr.Format == countnum)//28, 76
		{
			pContext->PSSetShader(xsAntiDark, NULL, NULL);
		}

	//log
	//if (Stride == 20 && countnum == veWidth/1)
	if (countnum == IndexCountPerInstance / 100)
		if (GetAsyncKeyState('I') & 1)
			Log("Stride == %d && IndexCountPerInstance == %d && inWidth == %d && veWidth == %d",
				Stride, IndexCountPerInstance, inWidth, veWidth);

	//if (Stride == 20 && countnum == veWidth / 1)
	if (countnum == IndexCountPerInstance / 100)
	{
		//pContext->PSSetShader(xsAntiDark, NULL, NULL);
		return;
	}

	if (GetAsyncKeyState('O') & 1) //-
		countnum--;
	if (GetAsyncKeyState('P') & 1) //+
		countnum++;
	if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState('9') & 1) //reset, set to -1
		countnum = -1;


	return orig_D3D11DrawIndexedInstanced(pContext, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

//==========================================================================================================================

DWORD WINAPI cryptsp(LPVOID lpParameter)
{
	do
	{
		Sleep(100);
	} while (!GetModuleHandleA("dxgi.dll"));
	Sleep(4000);
	
	if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &pDevice, NULL, &pContext))) {
		MessageBox(0, L"D3D11CreateDevice failed", NULL, MB_OK);
		return 1;
	}

	contextvtable = *(void***)pContext;
	devicevtable = *(void***)pDevice;

	orig_D3D11DrawIndexed = (D3D11DRAWINDEXED)(DWORD_PTR*)contextvtable[12];
	orig_D3D11DrawIndexedInstanced = (D3D11DRAWINDEXEDINSTANCED)(DWORD_PTR*)contextvtable[20];


	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(LPVOID&)orig_D3D11DrawIndexed, (PBYTE)hook_D3D11DrawIndexed);
	DetourAttach(&(LPVOID&)orig_D3D11DrawIndexedInstanced, (PBYTE)hook_D3D11DrawIndexedInstanced);
	DetourTransactionCommit();

	pDevice->Release();
	pContext->Release();
	
	return 0;
}

//==========================================================================================================================//

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(cryptsp), NULL, 0, 0);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		DetourDetach(&(LPVOID&)orig_D3D11DrawIndexed, (PBYTE)hook_D3D11DrawIndexed);
		DetourDetach(&(LPVOID&)orig_D3D11DrawIndexedInstanced, (PBYTE)hook_D3D11DrawIndexedInstanced);
		break;
	}
	return TRUE;
}
