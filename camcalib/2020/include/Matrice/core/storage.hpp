/**************************************************************************
This file is part of Matrice, an effcient and elegant C++ library.
Copyright(C) 2018, Zhilong(Dgelom) Su, all rights reserved.

This program is free software : you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/
#pragma once

#include "../private/_memory.h"
#include "../core/_expr_type_traits.h"
#include <memory>
#include <cmath>
#include <initializer_list>
#include <type_traits>
#if (defined __enable_cuda__ && !defined __disable_cuda__)
#define __disable_simd__
#include <host_defines.h>
#define MATRICE_HOST_ONLY __host__
#define MATRICE_DEVICE_ONLY __device__
#define MATRICE_GLOBAL __host__ __device__
#define MATRICE_HOST_INL __inline__ __host__
#define MATRICE_DEVICE_INL __inline__ __device__
#define MATRICE_GLOBAL_INL __inline__ __host__ __device__
#define MATRICE_HOST_FINL __forceinline__ __host__
#define MATRICE_DEVICE_FINL __forceinline__ __device__
#define MATRICE_GLOBAL_FINL __forceinline__ __host__ __device__
#else
#define MATRICE_HOST_ONLY
#define MATRICE_DEVICE_ONLY 
#define MATRICE_GLOBAL
#define MATRICE_HOST_INL inline
#define MATRICE_DEVICE_INL inline
#define MATRICE_GLOBAL_INL __inline
#define MATRICE_HOST_FINL __forceinline
#define MATRICE_DEVICE_FINL __forceinline
#define MATRICE_GLOBAL_FINL __forceinline
#endif

namespace Dgelo {
namespace details {
typedef enum Location 
{ 
	UnSpecified = -1, OnStack = 0, OnHeap = 1, OnDevice = 2, OnGlobal = 3 
} loctn_t;
template<typename _Ty> class Storage_
{
public:
#ifdef DGE_USE_SHARED_MALLOC
	using value_t   =            _Ty;
	using pointer   =         _Ty * ;
	using reference =         _Ty & ;
	using int_t     = std::ptrdiff_t;
	using idx_t     =    std::size_t;
#else
	typedef _Ty             value_t;
	typedef _Ty*            pointer;
	typedef _Ty&            reference;
	typedef std::size_t     idx_t;
	typedef std::ptrdiff_t  int_t;
#endif

	enum Ownership { Owner = 1, Refer = 0, Proxy = -1, Dummy = -2 };

	///<brief> data memory </brief>
	template<Location _Loc = UnSpecified> class DenseBase
	{
	public:
		MATRICE_GLOBAL_FINL DenseBase()
			: my_rows(0), my_cols(0), my_size(0), my_data(0), my_owner(Dummy) {}
		MATRICE_GLOBAL_FINL DenseBase(int_t _rows, int_t _cols, pointer _data)
			: my_rows(_rows), my_cols(_cols), my_size(my_rows*my_cols), my_data(_data), my_owner(_Loc == OnStack ? Owner : Proxy) {}
		MATRICE_GLOBAL DenseBase(int_t _rows, int_t _cols);
		MATRICE_GLOBAL DenseBase(int_t _rows, int_t _cols, const value_t _val);
		MATRICE_GLOBAL DenseBase(int_t _rows, int_t _cols, pointer _data, std::initializer_list<value_t> _list) noexcept
			:DenseBase(_rows, _cols, _data){
			for (auto _Idx = 0; _Idx < std::min(size_t(my_size), _list.size()); ++_Idx)
				my_data[_Idx] = *(_list.begin() + _Idx);
		}
		MATRICE_GLOBAL DenseBase(const DenseBase& _other);
		MATRICE_GLOBAL DenseBase(DenseBase&& _other);

		MATRICE_GLOBAL ~DenseBase() {
		}

		///<brief> operators </brief>
		MATRICE_GLOBAL DenseBase& operator=(const DenseBase& _other);
		MATRICE_GLOBAL DenseBase& operator=(DenseBase&& _other);
		MATRICE_GLOBAL DenseBase& operator=(std::initializer_list<value_t> _list);

		///<brief> methods </brief>
		MATRICE_GLOBAL_INL int_t& size() const { return (my_size);}
		MATRICE_GLOBAL_INL pointer data() const { return (my_data); }
		MATRICE_GLOBAL_INL Ownership& owner() const { return my_owner; }
		MATRICE_GLOBAL_INL bool shared() const 
		{
#ifdef DGE_USE_SHARED_MALLOC
			return (my_shared.get());
#else
			return 0;
#endif
		}

	protected:
		mutable int_t my_rows, my_cols;
		mutable int_t my_size;
		mutable pointer my_data;
		mutable Ownership my_owner = Owner;
	private:
#ifdef DGE_USE_SHARED_MALLOC
		using SharedPtr = std::shared_ptr<value_t>;
		SharedPtr my_shared;
#endif
		Location my_location = _Loc;
	};

	///<brief> generic allocator and its specialization </brief>
	template<int _M, int _N, int _Options> class Allocator;

	//Managed allocator
	template<int _M, int _N, int _Options> 
	class Allocator : public DenseBase<OnStack>
	{
		typedef DenseBase<OnStack> Base;
	public:
		MATRICE_HOST_INL Allocator() 
			: Base(_M, _N, _Data) {}
		MATRICE_HOST_INL Allocator(int ph1, int ph2, pointer data) 
			: Base(_M, _N, data) {}
		MATRICE_HOST_INL Allocator(std::initializer_list<value_t> _list) 
			: Base(_M, _N, _Data, _list) {}
		MATRICE_HOST_INL Allocator(const Allocator& _other) 
			: Base(_M, _N, privt::fill_mem(_other._Data, _Data, _other.my_size)) {}

		MATRICE_HOST_INL Allocator& operator= (const Allocator& _other)
		{
			Base::my_data = _Data;
			Base::my_cols = _other.my_cols;
			Base::my_rows = _other.my_rows;
			Base::my_size = _other.my_size;
			Base::my_owner = _other.my_owner;
			privt::fill_mem(_other._Data, _Data, Base::my_size);
			return (*this);
		}

	private:
		value_t alignas(MATRICE_ALIGN_BYTES)_Data[_M*_N];
	};

	//Dynamic allocator
	template<int _Options> 
	class Allocator<0, 0, _Options> : public DenseBase<OnHeap>
	{
		typedef DenseBase<OnHeap> Base;
	public:
		MATRICE_HOST_INL Allocator() : Base() {}
		MATRICE_HOST_INL Allocator(int _m, int _n) : Base(_m, _n) {}
		MATRICE_HOST_INL Allocator(int _m, int _n, const value_t _val) : Base(_m, _n, _val) {}
		MATRICE_HOST_INL Allocator(int _m, int _n, pointer data) : Base(_m, _n, data) {}
		MATRICE_HOST_INL Allocator(const Allocator& _other) : Base(_other) {}
		MATRICE_HOST_INL Allocator(Allocator&& _other) : Base(std::move(_other)) {}
		MATRICE_HOST_INL Allocator(std::initializer_list<value_t> _list) {}

		MATRICE_HOST_INL Allocator& operator= (const Allocator& _other) { return static_cast<Allocator&>(Base::operator= (_other)); }
		MATRICE_HOST_INL Allocator& operator= (Allocator&& _other) { return static_cast<Allocator&>(Base::operator=(std::move(_other))); }
	};

	//Unified allocator
	template<int _Options> 
	class Allocator<-1, 0, _Options> : public DenseBase<OnGlobal>
	{
		typedef DenseBase<OnGlobal> Base;
	public:
		MATRICE_HOST_INL Allocator() : Base() {}
		MATRICE_HOST_INL Allocator(int _m, int _n) : Base(_m, _n) {}
		MATRICE_HOST_INL Allocator(int _m, int _n, pointer data) : Base(_m, _n, data) {}
		MATRICE_HOST_INL Allocator(int _m, int _n, const value_t _val) : Base(_m, _n, _val) {}
		MATRICE_HOST_INL Allocator(const Allocator& _other) : Base(_other) {}
		MATRICE_HOST_INL Allocator(Allocator&& _other) : Base(std::move(_other)) {}
		MATRICE_HOST_INL Allocator(std::initializer_list<value_t> _list) {}

		MATRICE_HOST_INL Allocator& operator= (const Allocator& _other) { return static_cast<Allocator&>(Base::operator= (_other)); }
	};
	//Device allocator
	template<int _Options> 
	class Allocator<-1, -1, _Options> : public DenseBase<OnDevice>
	{
		typedef DenseBase<OnDevice> Base;
	public:
		MATRICE_DEVICE_INL Allocator() : Base() {}
		MATRICE_DEVICE_INL Allocator(int _m, int _n) : Base(_m, _n) {}
		MATRICE_DEVICE_INL Allocator(int _m, int _n, pointer data) : Base(_m, _n, data) {}
		MATRICE_DEVICE_INL Allocator(int _m, int _n, const value_t _val) : Base(_m, _n, _val) {}
		MATRICE_DEVICE_INL Allocator(const Allocator& _other) : Base(_other) {}
		MATRICE_DEVICE_INL Allocator(Allocator&& _other) : Base(std::move(_other)) {}
		MATRICE_DEVICE_INL Allocator(std::initializer_list<value_t> _list) {}

		MATRICE_DEVICE_INL Allocator& operator= (const Allocator& _other) { return static_cast<Allocator&>(Base::operator= (_other)); }
	};


	///<brief> specialization allocator classes </brief>
	template<class DerivedAllocator, Location _Loc> class Base_
	{
#ifdef DGE_USE_SHARED_MALLOC
		using SharedPtr = std::shared_ptr<value_t>;
#endif
	public:
		Base_() 
			: m_rows(0), m_cols(0), m_location(UnSpecified){}
		Base_(int rows, int cols) 
			: m_rows(rows), m_cols(cols), m_data(privt::aligned_malloc<value_t>(rows*cols)) {}
#ifdef DGE_USE_SHARED_MALLOC
		Base_(int rows, int cols, bool is_shared) 
			: m_rows(rows), m_cols(cols), m_shared(is_shared)
		{
			_SharedData = SharedPtr(privt::aligned_malloc<value_t>(rows*cols), 
				[=](pointer ptr) { privt::aligned_free(ptr); });
			m_data = _SharedData.get();
		}
#endif
		~Base_()
		{
			if ((!m_shared) && m_data) privt::aligned_free(m_data);
		}

#pragma region operator overload
		MATRICE_GLOBAL_INL
		reference operator[](idx_t idx) const { return m_data[idx]; }
#pragma endregion
#pragma region public methods
		MATRICE_GLOBAL_INL std::size_t cols() const { return m_cols; }
		MATRICE_GLOBAL_INL std::size_t rows() const { return m_rows; }
		MATRICE_GLOBAL_INL std::size_t size() const { return m_rows * m_cols; }
		MATRICE_GLOBAL_INL pointer data() const { return (m_data); }
		MATRICE_GLOBAL_INL pointer ptr(idx_t ridx) const { return (m_data + m_cols*ridx); }
		MATRICE_GLOBAL_INL loctn_t& location() const { return m_location; }
#pragma endregion

	protected:
#ifdef DGE_USE_SHARED_MALLOC
		
		pointer m_data = nullptr;
#else      
		pointer m_data;
#endif
		mutable Location m_location = _Loc;
		std::size_t m_rows, m_cols;
		bool m_shared = false;
	private:
#ifdef DGE_USE_SHARED_MALLOC
		SharedPtr _SharedData = nullptr;
#endif
	};
	template<int M, int N> class ManagedAllocator : public Base_<ManagedAllocator<M, N>, OnStack>
	{
		using _Base = Base_<ManagedAllocator, OnStack>;
		using _Base::m_rows;
		using _Base::m_cols;
		typedef pointer MyPtr;
	public:
		ManagedAllocator() 
			: _Base(M, N) { m_data = &_Data[0]; }
		ManagedAllocator(int _m, int _n) 
			: _Base(M, N) { m_data = &_Data[0]; }

	private:
		value_t alignas(MATRICE_ALIGN_BYTES) _Data[M*N];
		using _Base::m_data;
	};
	template<typename = std::enable_if<std::is_arithmetic<_Ty>::value, _Ty>::type>
	class DynamicAllocator : public Base_<DynamicAllocator<_Ty>, OnHeap>
	{
		using _Base = Base_<DynamicAllocator<_Ty>, OnHeap>;
		using _Base::m_rows;
		using _Base::m_cols;
		typedef pointer MyPtr;
	public:
		DynamicAllocator() :_Base() {}
		template<typename IntegerType>
		DynamicAllocator(const IntegerType& _M, const IntegerType& _N) :_Base(_M, _N) {}

	private:
		using _Base::m_data;
	};
#ifdef DGE_USE_SHARED_MALLOC
	template<typename = std::enable_if<std::is_arithmetic<_Ty>::value, _Ty>::type> 
	class SharedAllocator : public Base_<SharedAllocator<_Ty>, OnHeap>
	{
		using _Base = Base_<SharedAllocator<_Ty>, OnHeap>;
		using _Base::m_rows;
		using _Base::m_cols;
	public:
		SharedAllocator() : _Base() {}
		template<typename IntegerType>
		SharedAllocator(const IntegerType& _M, const IntegerType& _N) :_Base(_M, _N, true) {}

	private:
		using _Base::m_data;
	};
#endif
	template<typename = std::enable_if<std::is_arithmetic<_Ty>::value, _Ty>::type>
	class UnifiedAllocator : public Base_<UnifiedAllocator<_Ty>, OnGlobal>
	{
		using _Base = Base_<UnifiedAllocator<_Ty>, OnGlobal>;
		using _Base::m_rows;
		using _Base::m_cols;
		typedef pointer MyPtr;
	public:
		UnifiedAllocator() : _Base() {}
		template<typename IntegerType>
		UnifiedAllocator(const IntegerType& _M, const IntegerType& _N) :_Base(_M, _N) {}

	private:
		using _Base::m_data;
	};
};
}
}

