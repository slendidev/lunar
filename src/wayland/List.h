#pragma once

#include <cstddef>
#include <iterator>

#include <wayland-server-core.h>

namespace Lunar::Wayland {

namespace detail {

template<typename T, wl_list T::*Member>
constexpr std::ptrdiff_t member_offset() noexcept
{
	return reinterpret_cast<std::ptrdiff_t>(
	    &(reinterpret_cast<T const volatile *>(0)->*Member));
}

template<typename T, wl_list T::*Member>
inline T *container_of(wl_list *node) noexcept
{
	auto *p = reinterpret_cast<std::byte *>(node) - member_offset<T, Member>();
	return reinterpret_cast<T *>(p);
}

template<typename T, wl_list T::*Member>
inline T const *container_of(wl_list const *node) noexcept
{
	auto *p = reinterpret_cast<std::byte const *>(node)
	    - member_offset<T, Member>();
	return reinterpret_cast<T const *>(p);
}

} // namespace detail

template<typename T, wl_list T::*Member> struct List {
	struct Iterator {
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		using pointer = T *;
		using reference = T &;

		Iterator() = default;
		Iterator(wl_list *cur, wl_list *head)
		    : m_cur(cur)
		    , m_head(head)
		{
		}

		auto operator*() const -> reference
		{
			return *detail::container_of<T, Member>(m_cur);
		}
		auto operator->() const -> pointer
		{
			return detail::container_of<T, Member>(m_cur);
		}

		auto operator++() -> Iterator &
		{
			m_cur = m_cur->next;
			return *this;
		}
		auto operator++(int) -> Iterator
		{
			auto t = *this;
			++(*this);
			return t;
		}

		auto operator--() -> Iterator &
		{
			m_cur = (m_cur == m_head) ? m_head->prev : m_cur->prev;
			return *this;
		}
		auto operator--(int) -> Iterator
		{
			auto t = *this;
			--(*this);
			return t;
		}

		friend auto operator==(Iterator a, Iterator b) -> bool
		{
			return a.m_cur == b.m_cur;
		}
		friend auto operator!=(Iterator a, Iterator b) -> bool
		{
			return !(a == b);
		}

	private:
		wl_list *m_cur { nullptr };
		wl_list *m_head { nullptr };
	};

	struct ConstIterator {
		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = T const;
		using difference_type = std::ptrdiff_t;
		using pointer = T const *;
		using reference = T const &;

		ConstIterator() = default;
		ConstIterator(wl_list const *cur, wl_list const *head)
		    : m_cur(cur)
		    , m_head(head)
		{
		}

		auto operator*() const -> reference
		{
			return *detail::container_of<T, Member>(m_cur);
		}
		auto operator->() const -> pointer
		{
			return detail::container_of<T, Member>(m_cur);
		}

		auto operator++() -> ConstIterator &
		{
			m_cur = m_cur->next;
			return *this;
		}
		auto operator++(int) -> ConstIterator
		{
			auto t = *this;
			++(*this);
			return t;
		}

		auto operator--() -> ConstIterator &
		{
			m_cur = (m_cur == m_head) ? m_head->prev : m_cur->prev;
			return *this;
		}
		auto operator--(int) -> ConstIterator
		{
			auto t = *this;
			--(*this);
			return t;
		}

		friend auto operator==(ConstIterator a, ConstIterator b) -> bool
		{
			return a.m_cur == b.m_cur;
		}
		friend auto operator!=(ConstIterator a, ConstIterator b) -> bool
		{
			return !(a == b);
		}

	private:
		wl_list const *m_cur { nullptr };
		wl_list const *m_head { nullptr };
	};

	List()
	    : m_head {}
	    , m_external_head { nullptr }
	    , m_should_cleanup { true }
	{
		wl_list_init(&m_head);
	}

	explicit List(
	    wl_list *existing_head, bool should_cleanup = false, bool init = false)
	    : m_head {}
	    , m_external_head { existing_head }
	    , m_should_cleanup { should_cleanup }
	{
		if (init && m_external_head)
			wl_list_init(m_external_head);
	}

	~List()
	{
		if (!m_should_cleanup)
			return;

		clear();
		if (auto *h = head_ptr(); h)
			wl_list_init(h);
	}

	List(List const &) = delete;
	auto operator=(List const &) -> List & = delete;

	List(List &&other) noexcept { move_from(other); }

	auto operator=(List &&other) noexcept -> List &
	{
		if (this != &other) {
			this->~List();
			move_from(other);
		}
		return *this;
	}

	inline auto c_ptr() -> wl_list * { return head_ptr(); }
	inline auto c_ptr() const -> wl_list const * { return head_ptr(); }

	auto empty() const noexcept -> bool { return wl_list_empty(head_ptr()); }
	auto length() const noexcept -> int { return wl_list_length(head_ptr()); }

	auto push_front(T *elem) noexcept -> void
	{
		wl_list_insert(head_ptr(), &(elem->*Member));
	}

	auto push_back(T *elem) noexcept -> void
	{
		auto *h = head_ptr();
		wl_list_insert(h->prev, &(elem->*Member));
	}

	auto remove(T *elem) noexcept -> void
	{
		wl_list_remove(&(elem->*Member));
		wl_list_init(&(elem->*Member));
	}

	auto clear() noexcept -> void
	{
		auto *h = head_ptr();
		while (!wl_list_empty(h)) {
			auto *node = h->next;
			wl_list_remove(node);
			wl_list_init(node);
		}
	}

	auto begin() noexcept -> Iterator
	{
		auto *h = head_ptr();
		return Iterator(h->next, h);
	}

	auto end() noexcept -> Iterator
	{
		auto *h = head_ptr();
		return Iterator(h, h);
	}

	auto begin() const noexcept -> ConstIterator
	{
		auto const *h = head_ptr();
		return ConstIterator(h->next, h);
	}

	auto end() const noexcept -> ConstIterator
	{
		auto const *h = head_ptr();
		return ConstIterator(h, h);
	}

private:
	auto head_ptr() noexcept -> wl_list *
	{
		return m_external_head ? m_external_head : &m_head;
	}

	auto head_ptr() const noexcept -> wl_list const *
	{
		return m_external_head ? m_external_head : &m_head;
	}

	auto move_from(List &other) noexcept -> void
	{
		m_head = other.m_head;
		m_external_head = other.m_external_head;
		m_should_cleanup = other.m_should_cleanup;

		other.m_external_head = nullptr;
		other.m_should_cleanup = false;
		wl_list_init(&other.m_head);
	}

private:
	wl_list m_head {};
	wl_list *m_external_head { nullptr };
	bool m_should_cleanup { true };
};

} // namespace Lunar::Wayland
