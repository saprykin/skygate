#include "SkyCatalogSourceOperationRegistry.hpp"

#include <utility>

namespace skygate::ui::internal {

SkyCatalogSourceOperation* SkyCatalogSourceOperationRegistry::upsert(
    const SkyCatalogSourceInstance& attempt, const skygate::ephemeris::CatalogCompositionPolicy policy
)
{
    // Requesting a configuration records it as the instance's attempt without
    // touching the accepted facts: an attempt that is never accepted must not
    // replace what persistence and consumers still see as the source.
    SkyCatalogSourceOperation* existing = find(attempt.instanceId);
    if (existing != nullptr) {
        existing->beginAttempt(attempt, policy);
        return existing;
    }

    m_operations.push_back(SkyCatalogSourceOperation(attempt, policy, allocateRevision()));
    return &m_operations.back();
}

SkyCatalogSourceOperation* SkyCatalogSourceOperationRegistry::find(const QString& instanceId)
{
    for (SkyCatalogSourceOperation& operation : m_operations) {
        if (operation.attempt().instanceId == instanceId) {
            return &operation;
        }
    }
    return nullptr;
}

const SkyCatalogSourceOperation* SkyCatalogSourceOperationRegistry::find(const QString& instanceId) const
{
    for (const SkyCatalogSourceOperation& operation : m_operations) {
        if (operation.attempt().instanceId == instanceId) {
            return &operation;
        }
    }
    return nullptr;
}

bool SkyCatalogSourceOperationRegistry::isCurrent(const QString& instanceId, const std::uint64_t revision) const
{
    const SkyCatalogSourceOperation* operation = find(instanceId);
    return operation != nullptr && operation->revision() == revision;
}

void SkyCatalogSourceOperationRegistry::remove(const QString& instanceId)
{
    for (qsizetype index = 0; index < m_operations.size(); ++index) {
        if (m_operations[index].attempt().instanceId == instanceId) {
            retireOperation(m_operations[index]);
            m_operations.removeAt(index);
            return;
        }
    }
}

void SkyCatalogSourceOperationRegistry::retire(const QString& instanceId)
{
    SkyCatalogSourceOperation* operation = find(instanceId);
    if (operation == nullptr) {
        return;
    }

    retireOperation(*operation);
}

void SkyCatalogSourceOperationRegistry::retireAll()
{
    for (SkyCatalogSourceOperation& operation : m_operations) {
        retireOperation(operation);
    }
}

bool SkyCatalogSourceOperationRegistry::hasPendingRelatedWork() const noexcept
{
    for (const SkyCatalogSourceOperation& operation : m_operations) {
        if (operation.hasPendingRelatedWork()) {
            return true;
        }
    }
    return false;
}

void SkyCatalogSourceOperationRegistry::markPayloadEvicted(const QString& instanceId)
{
    if (SkyCatalogSourceOperation* operation = find(instanceId)) {
        operation->markPayloadEvicted();
    }
}

void SkyCatalogSourceOperationRegistry::replace(QVector<SkyCatalogSourceOperation> operations)
{
    retireAll();
    m_operations = std::move(operations);
}

void SkyCatalogSourceOperationRegistry::retireOperation(SkyCatalogSourceOperation& operation)
{
    operation.retire(allocateRevision());
}

}  // namespace skygate::ui::internal
