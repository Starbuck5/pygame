#include <stdio.h>

#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>

int _win_get_count() {
    IMFAttributes *pAttributes = NULL;
    IMFActivate **ppDevices = NULL;

    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) {
        printf("oof\n");
    }

    hr = pAttributes->lpVtbl->SetGUID(pAttributes, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
                              &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    if (FAILED(hr)) {
        printf("oof2\n");
    }

    UINT32 count = -1;
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);

    if (FAILED(hr)) {
        printf("oof3\n");
    }

    printf("This computer has %i cameras\n", count);

    return count;
}