
#include "D3DApp.h"
#include "Core/Core.h"

#include "SkinMesh/SkeletonMesh.h"

void InitAllGlobalData(D3DApp& theApp)
{
	//Begin Initialize Init All App Global parameters herer

	FileHelper::GetProjectPath(gSystemPath);
	gAppWindowWidth = theApp.GetClientWidth();
	gAppWindowHeight = theApp.GetClientHeight();
	//End Initialize
}

//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
//	PSTR cmdLine, int showCmd)
int main()
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		D3DApp theApp(nullptr);

		InitAllGlobalData(theApp);

		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}
