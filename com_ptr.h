#pragma once

#include "framework.h"
#include <type_traits>
#include <combaseapi.h>


namespace std
{
	template <>
	struct hash<IID>
	{
		size_t operator()(const IID& iid) const
		{
			const uint64_t* data = reinterpret_cast<const uint64_t*>(&iid);
			return static_cast<size_t>(data[0] ^ data[1]);
		}
	};
}

template <typename Interface, typename std::enable_if<std::is_base_of<IUnknown, Interface>::value, int>::type = 0>
class com_ptr
{
private:
	Interface* ptr_ = nullptr;

	void add_ref()
	{
		if (ptr_ != nullptr)
		{
			ptr_->AddRef();
		}
	}
	void release()
	{
		if (ptr_ != nullptr)
		{
			ptr_->Release();
			ptr_ = nullptr;
		}
	}
protected:
	using base = com_ptr<Interface>;
	Interface* ptr()
	{
		return ptr_;
	}
public:
	typedef Interface interface_type;

	void* operator new(std::size_t) = delete;
	void operator delete(void*) noexcept = delete;

	virtual ~com_ptr() noexcept
	{
		release();
	}
	com_ptr(Interface* ptr = nullptr) noexcept : ptr_(ptr)
	{
	}
	com_ptr(const com_ptr<Interface>& ref) noexcept : ptr_(ref.ptr_)
	{
		add_ref();
	}
	com_ptr(com_ptr<Interface>&& ref) noexcept : ptr_(ref.ptr_)
	{
		ref.ptr_ = nullptr;
	}
	com_ptr<Interface>& operator =(nullptr_t) noexcept
	{
		release();
		return *this;
	}
	com_ptr<Interface>& operator =(const com_ptr<Interface>& ref) noexcept
	{
		release();
		ptr_ = ref.ptr_;
		add_ref();
		return *this;
	}
	com_ptr<Interface>& operator =(com_ptr<Interface>&& ref) noexcept
	{
		release();
		ptr_ = ref.ptr_;
		ref.ptr_ = nullptr;
		return *this;
	}
	com_ptr<Interface>& operator =(Interface* ptr) noexcept
	{
		release();
		ptr_ = ptr;
		add_ref();
		return *this;
	}
	bool operator ==(const com_ptr<Interface>& ref) const noexcept
	{
		return ptr_ == ref.ptr_;
	}
	bool operator !=(const com_ptr<Interface>& ref) const noexcept
	{
		return ptr_ != ref.ptr_;
	}
	bool operator ==(nullptr_t) const noexcept
	{
		return ptr_ == nullptr;
	}
	bool operator !=(nullptr_t) const noexcept
	{
		return ptr_ != nullptr;
	}
	Interface** operator &() noexcept
	{
		release();
		return &ptr_;
	}
	Interface* operator ->() noexcept
	{
		return ptr_;
	}
	const Interface* operator ->() const noexcept
	{
		return ptr_;
	}
	operator Interface* () noexcept
	{
		return ptr_;
	}
	operator const Interface* () const noexcept
	{
		return ptr_;
	}
	template <typename T>
	com_ptr<T> query(const IID& iid) const noexcept
	{
		if (ptr_ == nullptr)
		{
			return com_ptr<T>();
		}
		com_ptr<T> pp;
		ptr_->QueryInterface(iid, reinterpret_cast<void**>(&pp));
		return pp;
	}
	constexpr const GUID& guid() const noexcept
	{
		return __uuidof(Interface);
	}
	void clear() noexcept
	{
		release();
	}
	bool empty() const noexcept
	{
		return ptr_ == nullptr;
	}
};
