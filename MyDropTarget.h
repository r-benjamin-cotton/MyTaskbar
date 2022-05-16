#pragma once

#include "framework.h"
#include "com_ptr.h"
#include "com_unk.h"

class MyDropTarget : public IDropTarget, private com_unk
{
private:
    virtual ~MyDropTarget() override
    {
        //DebugPrintf(L"destruct!\n");
    }
#ifdef _DEBUG
    std::wstring GetEffectString(DWORD dwEffect)
    {
        std::wstring str;
        if (dwEffect == 0)
        {
            str = L"DROPEFFECT_NONE ";
        }
        if ((dwEffect & DROPEFFECT_COPY) != 0)
        {
            str += L"DROPEFFECT_COPY ";
        }
        if ((dwEffect & DROPEFFECT_MOVE) != 0)
        {
            str += L"DROPEFFECT_MOVE ";
        }
        if ((dwEffect & DROPEFFECT_LINK) != 0)
        {
            str += L"DROPEFFECT_LINK ";
        }
        if ((dwEffect & DROPEFFECT_SCROLL) != 0)
        {
            str += L"DROPEFFECT_SCROLL ";
        }
        if (!str.empty())
        {
            return str.substr(0, str.size() - 1);
        }
        return FormatString(L"DROPEFFECT_%08x", dwEffect);
    }
    std::wstring GetKeyStateString(DWORD dwKeyState)
    {
        std::wstring str;
        if (dwKeyState == 0)
        {
            str = L"MK_NULL ";
        }
        if ((dwKeyState & MK_LBUTTON) != 0)
        {
            str += L"MK_LBUTTON ";
        }
        if ((dwKeyState & MK_RBUTTON) != 0)
        {
            str += L"MK_RBUTTON ";
        }
        if ((dwKeyState & MK_SHIFT) != 0)
        {
            str += L"MK_SHIFT ";
        }
        if ((dwKeyState & MK_CONTROL) != 0)
        {
            str += L"MK_CONTROL ";
        }
        if ((dwKeyState & MK_MBUTTON) != 0)
        {
            str += L"MK_MBUTTON ";
        }
#if 0
        if ((dwKeyState & MK_XBUTTON1) != 0)
        {
            str += L"MK_XBUTTON1 ";
        }
#else
        if ((dwKeyState & MK_ALT) != 0)
        {
            str += L"MK_ALT ";
        }
#endif
        if ((dwKeyState & MK_XBUTTON2) != 0)
        {
            str += L"MK_XBUTTON2 ";
        }
        if (!str.empty())
        {
            return str.substr(0, str.size() - 1);
        }
        return FormatString(L"MK_%08x", dwKeyState);
    }
    
#endif
public:
    MyDropTarget()
    {
        register_interface(IID_IUnknown, this);
        register_interface(IID_IDropTarget, this);
    }
    MyDropTarget(const MyDropTarget& ref) noexcept = delete;
    MyDropTarget(MyDropTarget&& ref) noexcept = delete;
    MyDropTarget& operator =(const MyDropTarget& ref) noexcept = delete;
    MyDropTarget& operator =(MyDropTarget&& ref) noexcept = delete;
    virtual HRESULT STDMETHODCALLTYPE IUnknown::QueryInterface(REFIID iid, LPVOID* ppv) override
    {
        return com_unk::QueryInterface(iid, ppv);
    }
    virtual ULONG STDMETHODCALLTYPE IUnknown::AddRef() override
    {
        auto count = com_unk::AddRef();
        //DebugPrintf(L"%d\n", count);
        return count;
    }
    virtual ULONG STDMETHODCALLTYPE IUnknown::Release() override
    {
        auto count = com_unk::Release();
        //DebugPrintf(L"%d\n", count);
        return count;
    }

    virtual HRESULT STDMETHODCALLTYPE IDropTarget::DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
    {
        DebugPrintf(L"DragEnter(%s [%d,%d], %s)\n", GetKeyStateString(grfKeyState).c_str(), pt.x, pt.y, GetEffectString(*pdwEffect).c_str());
        if (OnDragEnter)
        {
            return OnDragEnter(pDataObj, grfKeyState, pt, pdwEffect);
        }
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE IDropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
    {
        //DebugPrintf(L"DragOver(%s [%d,%d], %s)\n", GetKeyStateString(grfKeyState).c_str(), pt.x, pt.y, GetEffectString(*pdwEffect).c_str());
        if (OnDragOver)
        {
            return OnDragOver(grfKeyState, pt, pdwEffect);
        }
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE IDropTarget::DragLeave() override
    {
        DebugPrintf(L"DragLeave()\n");
        if (OnDragLeave)
        {
            return OnDragLeave();
        }
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE IDropTarget::Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
    {
        DebugPrintf(L"Drop(%s [%d,%d], %s)\n", GetKeyStateString(grfKeyState).c_str(), pt.x, pt.y, GetEffectString(*pdwEffect).c_str());
        if (OnDrop)
        {
            return OnDrop(pDataObj, grfKeyState, pt, pdwEffect);
        }
        return S_OK;
    }

    std::function<HRESULT(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)> OnDragEnter;
    std::function<HRESULT(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)> OnDragOver;
    std::function<HRESULT()> OnDragLeave;
    std::function<HRESULT(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)> OnDrop;
};

