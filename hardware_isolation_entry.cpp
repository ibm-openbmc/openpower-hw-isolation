// SPDX-License-Identifier: Apache-2.0

#include "hardware_isolation_entry.hpp"

namespace hw_isolation
{
namespace entry
{

Entry::Entry(sdbusplus::bus::bus& bus, const std::string& objPath,
             const EntryId entryId, const EntryRecordId entryRecordId,
             const EntrySeverity isolatedHwSeverity,
             const EntryResolved entryIsResolved,
             const AssociationDef& associationDef) :
    type::ServerObject<EntryInterface, AssociationDefInterface>(
        bus, objPath.c_str(), true),
    _entryId(entryId), _entryRecordId(entryRecordId)
{
    // Setting properties which are defined in EntryInterface
    severity(isolatedHwSeverity);
    resolved(entryIsResolved);
    associations(associationDef);

    // Emit the signal for entry object creation since it deferred in
    // interface constructor
    this->emit_object_added();
}

} // namespace entry
} // namespace hw_isolation
