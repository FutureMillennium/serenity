/*
 * Copyright (c) 2020-2022, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibPartition/EBRPartitionTable.h>

#ifndef KERNEL
#    include <LibCore/DeprecatedFile.h>
#endif

namespace Partition {

#ifdef KERNEL
ErrorOr<NonnullOwnPtr<EBRPartitionTable>> EBRPartitionTable::try_to_initialize(Kernel::StorageDevice& device)
{
    auto table = TRY(adopt_nonnull_own_or_enomem(new (nothrow) EBRPartitionTable(device)));
#else
ErrorOr<NonnullOwnPtr<EBRPartitionTable>> EBRPartitionTable::try_to_initialize(NonnullRefPtr<Core::DeprecatedFile> device_file)
{
    auto table = TRY(adopt_nonnull_own_or_enomem(new (nothrow) EBRPartitionTable(move(device_file))));
#endif
    if (table->is_protective_mbr())
        return Error::from_errno(ENOTSUP);
    if (!table->is_valid())
        return Error::from_errno(EINVAL);
    return table;
}

#ifdef KERNEL
void EBRPartitionTable::search_extended_partition(Kernel::StorageDevice& device, MBRPartitionTable& checked_ebr, u64 current_block_offset, size_t limit)
#else
void EBRPartitionTable::search_extended_partition(NonnullRefPtr<Core::DeprecatedFile> device, MBRPartitionTable& checked_ebr, u64 current_block_offset, size_t limit)
#endif
{
    if (limit == 0)
        return;
    // EBRs should not carry more than 2 partitions (because they need to form a linked list)
    VERIFY(checked_ebr.partitions_count() <= 2);
    auto checked_logical_partition = checked_ebr.partition(0);

    // If we are pointed to an invalid logical partition, something is seriously wrong.
    VERIFY(checked_logical_partition.has_value());
    m_partitions.append(checked_logical_partition.value().offset(current_block_offset));
    if (!checked_ebr.contains_ebr())
        return;
    current_block_offset += checked_ebr.partition(1).value().start_block();
    auto next_ebr = MBRPartitionTable::try_to_initialize(device, current_block_offset);
    if (!next_ebr)
        return;
    search_extended_partition(device, *next_ebr, current_block_offset, (limit - 1));
}

#ifdef KERNEL
EBRPartitionTable::EBRPartitionTable(Kernel::StorageDevice& device)
#else
EBRPartitionTable::EBRPartitionTable(NonnullRefPtr<Core::DeprecatedFile> device)
#endif
    : MBRPartitionTable(device)
{
    if (!is_header_valid())
        return;
    m_valid = true;

    VERIFY(partitions_count() == 0);

    auto& header = this->header();
    for (size_t index = 0; index < 4; index++) {
        auto& entry = header.entry[index];
        // Start enumerating all logical partitions
        if (entry.type == 0xf) {
            auto checked_ebr = MBRPartitionTable::try_to_initialize(device, entry.offset);
            if (!checked_ebr)
                continue;
            // It's quite unlikely to see that amount of partitions, so stop at 128 partitions.
            search_extended_partition(device, *checked_ebr, entry.offset, 128);
            continue;
        }

        if (entry.offset == 0x00) {
            continue;
        }
        MUST(m_partitions.try_empend(entry.offset, (entry.offset + entry.length) - 1, entry.type));
    }
}

EBRPartitionTable::~EBRPartitionTable() = default;

}
