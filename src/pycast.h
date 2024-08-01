/*
 * This file is part of liblcf. Copyright (c) liblcf authors.
 * https://github.com/EasyRPG/liblcf - https://easyrpg.org
 *
 * liblcf is Free/Libre Open Source Software, released under the MIT License.
 * For the full copyright and license information, please view the COPYING
 * file that was distributed with this source code.
 */

#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/cast.h>
#include <pybind11/detail/common.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>

#include "lcf/dbarray.h"
#include "lcf/dbbitarray.h"
#include "lcf/dbstring.h"

namespace PYBIND11_NAMESPACE { namespace detail {
	template <> struct type_caster<lcf::DBString> : public string_caster<lcf::DBString> {};
	template <typename Type, typename Value> struct immutable_list_caster {
		using value_conv = make_caster<Value>;

		bool load(handle src, bool convert) {
			if (!isinstance<sequence>(src) || isinstance<bytes>(src) || isinstance<str>(src))
				return false;
			auto s = reinterpret_borrow<sequence>(src);
			value = Type(s.size());
			int index = 0;
			for (auto it : s) {
				value_conv conv;
				if (!conv.load(it, convert))
					return false;
				value[index++] = cast_op<Value &&>(std::move(conv));
			}
			return true;
		}

		template <typename T>
		static handle cast(T &&src, return_value_policy policy, handle parent) {
			if (!std::is_lvalue_reference<T>::value)
				policy = return_value_policy_override<Value>::policy(policy);
			list l(src.size());
			ssize_t index = 0;
			for (auto &&value : src) {
				auto value_ = reinterpret_steal<object>(value_conv::cast(forward_like<T>(value), policy, parent));
				if (!value_)
					return handle();
				PyList_SET_ITEM(l.ptr(), index++, value_.release().ptr()); // steals a reference
			}
			return l.release();
		}

		PYBIND11_TYPE_CASTER(Type, const_name("List[") + value_conv::name + const_name("]"));
	};
	template <typename T> struct type_caster<lcf::DBArray<T>> : public immutable_list_caster<lcf::DBArray<T>, T> {};
	template <> struct type_caster<lcf::DBBitArray> : public immutable_list_caster<lcf::DBBitArray, bool> {};
}}
