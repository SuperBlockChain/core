#pragma once
#include <xdr/SuperBlockChain-types.h>

namespace std
{
template <> struct hash<snb::uint256>
{
    size_t operator()(snb::uint256 const& x) const noexcept;
};
}
