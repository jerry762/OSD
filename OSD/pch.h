#pragma once

#include <Windows.h>
#include <d2d1_3.h>
#include <dxgi1_6.h>
#include <d3d11.h>
#include <Shlwapi.h>
#include <d2d1svg.h>


template <class T> void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}