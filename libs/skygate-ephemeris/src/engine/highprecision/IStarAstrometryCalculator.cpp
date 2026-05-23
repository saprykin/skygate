#include "engine/highprecision/IStarAstrometryCalculator.hpp"

#include <QtGlobal>

#include <vector>

namespace skygate::ephemeris::highprecision {

std::vector<StarAstrometryBatchResult> IStarAstrometryCalculator::calculateBatch(
    const EphemerisRequest& request,
    const CatalogStarAstrometryArrays& arrays,
    std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState
) const
{
    Q_UNUSED(request);
    Q_UNUSED(arrays);
    Q_UNUSED(preparedRequestState);
    return {};
}

}  // namespace skygate::ephemeris::highprecision
