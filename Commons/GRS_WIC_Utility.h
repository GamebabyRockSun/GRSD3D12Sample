#pragma once
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <wincodec.h> //for WIC
#include <d3d12.h> //for d3d12
#include <wrl.h> //添加WTL支持 方便使用COM
#include "GRS_Def.h"
#include "GRS_Mem.h"

using namespace Microsoft;
using namespace Microsoft::WRL;

struct WICTranslate
{
	GUID wic;
	DXGI_FORMAT format;
};

static WICTranslate g_WICFormats[] =
{//WIC格式与DXGI像素格式的对应表，该表中的格式为被支持的格式
	{ GUID_WICPixelFormat128bppRGBAFloat,       DXGI_FORMAT_R32G32B32A32_FLOAT },

	{ GUID_WICPixelFormat64bppRGBAHalf,         DXGI_FORMAT_R16G16B16A16_FLOAT },
	{ GUID_WICPixelFormat64bppRGBA,             DXGI_FORMAT_R16G16B16A16_UNORM },

	{ GUID_WICPixelFormat32bppRGBA,             DXGI_FORMAT_R8G8B8A8_UNORM },
	{ GUID_WICPixelFormat32bppBGRA,             DXGI_FORMAT_B8G8R8A8_UNORM }, // DXGI 1.1
	{ GUID_WICPixelFormat32bppBGR,              DXGI_FORMAT_B8G8R8X8_UNORM }, // DXGI 1.1

	{ GUID_WICPixelFormat32bppRGBA1010102XR,    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM }, // DXGI 1.1
	{ GUID_WICPixelFormat32bppRGBA1010102,      DXGI_FORMAT_R10G10B10A2_UNORM },

	{ GUID_WICPixelFormat16bppBGRA5551,         DXGI_FORMAT_B5G5R5A1_UNORM },
	{ GUID_WICPixelFormat16bppBGR565,           DXGI_FORMAT_B5G6R5_UNORM },

	{ GUID_WICPixelFormat32bppGrayFloat,        DXGI_FORMAT_R32_FLOAT },
	{ GUID_WICPixelFormat16bppGrayHalf,         DXGI_FORMAT_R16_FLOAT },
	{ GUID_WICPixelFormat16bppGray,             DXGI_FORMAT_R16_UNORM },
	{ GUID_WICPixelFormat8bppGray,              DXGI_FORMAT_R8_UNORM },

	{ GUID_WICPixelFormat8bppAlpha,             DXGI_FORMAT_A8_UNORM },
};

// WIC 像素格式转换表.
struct WICConvert
{
	GUID source;
	GUID target;
};

static WICConvert g_WICConvert[] =
{
	// 目标格式一定是最接近的被支持的格式
	{ GUID_WICPixelFormatBlackWhite,            GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

	{ GUID_WICPixelFormat1bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat2bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat4bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat8bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

	{ GUID_WICPixelFormat2bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM
	{ GUID_WICPixelFormat4bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

	{ GUID_WICPixelFormat16bppGrayFixedPoint,   GUID_WICPixelFormat16bppGrayHalf }, // DXGI_FORMAT_R16_FLOAT
	{ GUID_WICPixelFormat32bppGrayFixedPoint,   GUID_WICPixelFormat32bppGrayFloat }, // DXGI_FORMAT_R32_FLOAT

	{ GUID_WICPixelFormat16bppBGR555,           GUID_WICPixelFormat16bppBGRA5551 }, // DXGI_FORMAT_B5G5R5A1_UNORM

	{ GUID_WICPixelFormat32bppBGR101010,        GUID_WICPixelFormat32bppRGBA1010102 }, // DXGI_FORMAT_R10G10B10A2_UNORM

	{ GUID_WICPixelFormat24bppBGR,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat24bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat32bppPBGRA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat32bppPRGBA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

	{ GUID_WICPixelFormat48bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat48bppBGR,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppBGRA,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPRGBA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPBGRA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

	{ GUID_WICPixelFormat48bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat48bppBGRFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppBGRAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat48bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT

	{ GUID_WICPixelFormat128bppPRGBAFloat,      GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBFloat,        GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBAFixedPoint,  GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBFixedPoint,   GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat32bppRGBE,             GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT

	{ GUID_WICPixelFormat32bppCMYK,             GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat64bppCMYK,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat40bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat80bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

	{ GUID_WICPixelFormat32bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat64bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPRGBAHalf,        GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
};

bool GetTargetPixelFormat(const GUID* pSourceFormat, GUID* pTargetFormat)
{//查表确定兼容的最接近格式是哪个
	*pTargetFormat = *pSourceFormat;
	for (size_t i = 0; i < _countof(g_WICConvert); ++i)
	{
		if (InlineIsEqualGUID(g_WICConvert[i].source, *pSourceFormat))
		{
			*pTargetFormat = g_WICConvert[i].target;
			return true;
		}
	}
	return false;
}

DXGI_FORMAT GetDXGIFormatFromPixelFormat(const GUID* pPixelFormat)
{//查表确定最终对应的DXGI格式是哪一个
	for (size_t i = 0; i < _countof(g_WICFormats); ++i)
	{
		if (InlineIsEqualGUID(g_WICFormats[i].wic, *pPixelFormat))
		{
			return g_WICFormats[i].format;
		}
	}
	return DXGI_FORMAT_UNKNOWN;
}

// 加载纹理
__inline BOOL WICLoadImageFromFile(LPCWSTR pszTextureFile
	, DXGI_FORMAT& emTextureFormat
	, UINT& nTextureW
	, UINT& nTextureH
	, UINT& nPicRowPitch
	, BYTE*& pbImageData
	, size_t& szBufferSize )
{
	BOOL bRet = TRUE;
	try
	{
		static ComPtr<IWICImagingFactory>	pIWICFactory;
		ComPtr<IWICBitmapDecoder>			pIWICDecoder;
		ComPtr<IWICBitmapFrameDecode>		pIWICFrame;
		ComPtr<IWICBitmapSource>			pIBMP;

		UINT								nBPP = 0;

		USES_CONVERSION;
		//使用纯COM方式创建WIC类厂对象，也是调用WIC第一步要做的事情
		GRS_THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory
			, nullptr
			, CLSCTX_INPROC_SERVER
			, IID_PPV_ARGS(&pIWICFactory)));

		WCHAR pszTextureFileName[MAX_PATH] = {};
		StringCchCopyW(pszTextureFileName, MAX_PATH, pszTextureFile);

		GRS_THROW_IF_FAILED(pIWICFactory->CreateDecoderFromFilename(
			pszTextureFileName,						// 文件名
			NULL, 		// 不指定解码器，使用默认
			GENERIC_READ, // 访问权限
			WICDecodeMetadataCacheOnDemand,			// 若需要就缓冲数据 
			&pIWICDecoder // 解码器对象
		));

		// 获取第一帧图片(因为GIF等格式文件可能会有多帧图片，其他的格式一般只有一帧图片)
		// 实际解析出来的往往是位图格式数据
		GRS_THROW_IF_FAILED(pIWICDecoder->GetFrame(0, &pIWICFrame));

		WICPixelFormatGUID wpf = {};
		//获取WIC图片格式
		GRS_THROW_IF_FAILED(pIWICFrame->GetPixelFormat(&wpf));
		GUID tgFormat = {};

		//通过第一道转换之后获取DXGI的等价格式
		if (GetTargetPixelFormat(&wpf, &tgFormat))
		{
			emTextureFormat = GetDXGIFormatFromPixelFormat(&tgFormat);
		}

		if (DXGI_FORMAT_UNKNOWN == emTextureFormat)
		{// 不支持的图片格式 目前退出了事 
		 // 一般 在实际的引擎当中都会提供纹理格式转换工具，
		 // 图片都需要提前转换好，所以不会出现不支持的现象
			AtlThrow(S_FALSE);
		}

		if (!InlineIsEqualGUID(wpf, tgFormat))
		{// 这个判断很重要，如果原WIC格式不是直接能转换为DXGI格式的图片时
		 // 我们需要做的就是转换图片格式为能够直接对应DXGI格式的形式
			//创建图片格式转换器
			ComPtr<IWICFormatConverter> pIConverter;
			GRS_THROW_IF_FAILED(pIWICFactory->CreateFormatConverter(&pIConverter));

			//初始化一个图片转换器，实际也就是将图片数据进行了格式转换
			GRS_THROW_IF_FAILED(pIConverter->Initialize(
				pIWICFrame.Get(),                // 输入原图片数据
				tgFormat,						 // 指定待转换的目标格式
				WICBitmapDitherTypeNone,         // 指定位图是否有调色板，现代都是真彩位图，不用调色板，所以为None
				NULL,                            // 指定调色板指针
				0.f,                             // 指定Alpha阀值
				WICBitmapPaletteTypeCustom       // 调色板类型，实际没有使用，所以指定为Custom
			));
			// 调用QueryInterface方法获得对象的位图数据源接口
			GRS_THROW_IF_FAILED(pIConverter.As(&pIBMP));
		}
		else
		{
			//图片数据格式不需要转换，直接获取其位图数据源接口
			GRS_THROW_IF_FAILED(pIWICFrame.As(&pIBMP));
		}
		//获得图片大小（单位：像素）
		GRS_THROW_IF_FAILED(pIBMP->GetSize(&nTextureW, &nTextureH));

		//获取图片像素的位大小的BPP（Bits Per Pixel）信息，用以计算图片行数据的真实大小（单位：字节）
		ComPtr<IWICComponentInfo> pIWICmntinfo;
		GRS_THROW_IF_FAILED(pIWICFactory->CreateComponentInfo(tgFormat, pIWICmntinfo.GetAddressOf()));

		WICComponentType type;
		GRS_THROW_IF_FAILED(pIWICmntinfo->GetComponentType(&type));

		if (type != WICPixelFormat)
		{
			AtlThrow(S_FALSE);
		}

		ComPtr<IWICPixelFormatInfo> pIWICPixelinfo;
		GRS_THROW_IF_FAILED(pIWICmntinfo.As(&pIWICPixelinfo));

		// 到这里终于可以得到BPP了，这也是我看的比较吐血的地方，为了BPP居然饶了这么多环节
		GRS_THROW_IF_FAILED(pIWICPixelinfo->GetBitsPerPixel(&nBPP));

		// 计算图片实际的行大小（单位：字节），这里使用了一个上取整除法即（A+B-1）/B ，
		// 这曾经被传说是微软的面试题,希望你已经对它了如指掌
		nPicRowPitch = (UINT)GRS_UPPER_DIV(uint64_t(nTextureW) * uint64_t(nBPP), 8);

		size_t szImageBuffer = nPicRowPitch * nTextureH;

		//按照实际图片数据存储的内存大小
		BYTE* pbPicData = (BYTE*)GRS_CALLOC(szImageBuffer);
		if (nullptr == pbPicData)
		{
			AtlThrowLastWin32();
		}

		//从图片中读取出数据
		GRS_THROW_IF_FAILED(pIBMP->CopyPixels(nullptr
			, nPicRowPitch
			, static_cast<UINT>(szImageBuffer)
			, pbPicData) );

		pbImageData = pbPicData;
		szBufferSize = szImageBuffer;
	}
	catch (CAtlException& e)
	{//发生了COM异常
		e;
		bRet = FALSE;
	}
	catch (...)
	{
		bRet = FALSE;
	}

	return bRet;
}