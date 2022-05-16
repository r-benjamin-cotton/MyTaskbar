#pragma once

#include "framework.h"
#include <cassert>
#include <atomic>
#include <unordered_map>
#include <combaseapi.h>

class com_unk
{
private:
	std::atomic_ulong ref_count_;
	std::unordered_map<IID, IUnknown*> interface_map_;

protected:
	virtual ~com_unk() noexcept = 0
	{
	}

	void register_interface(const IID& iid, IUnknown* punk)
	{
		interface_map_.emplace(iid, punk);
	}

	HRESULT QueryInterface(REFIID iid, LPVOID* ppv)
	{
		auto it = interface_map_.find(iid);
		if (it == interface_map_.end())
		{
			*ppv = nullptr;
			return E_NOINTERFACE;
		}
		AddRef();
		*ppv = it->second;
		return S_OK;
	}
	ULONG AddRef() noexcept
	{
		assert(ref_count_ > 0);
		return ++ref_count_;
	}
	ULONG Release() noexcept
	{
		assert(ref_count_ > 0);
		if (0 != --ref_count_)
		{
			return ref_count_;
		}
		delete this;
		return 0;
	}

public:
	com_unk() noexcept : ref_count_(1)
	{
	}
	com_unk(const com_unk& ref) = delete;
	com_unk(com_unk&& ref) noexcept = delete;
	com_unk& operator =(const com_unk& ref) = delete;
	com_unk& operator =(com_unk&& ref) noexcept = delete;
};
