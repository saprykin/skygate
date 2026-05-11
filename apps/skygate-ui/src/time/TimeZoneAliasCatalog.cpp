#include "TimeZoneAliasCatalog.hpp"

#include <array>

namespace {

struct TimeZoneAliasEntry final {
    const char* timeZoneId;
    const char* aliases;
};

constexpr auto kTimeZoneAliases = std::to_array<TimeZoneAliasEntry>({
    {"UTC", "GMT|Zulu|Z time|Coordinated Universal Time"},

    {"America/New_York", "EST|EDT|Eastern Time|ET|New York|NYC|Boston|Washington DC"},
    {"America/Toronto", "EST|EDT|Eastern Time|ET|Toronto|Montreal|Ottawa"},
    {"America/Chicago", "CST|CDT|Central Time|CT|Chicago|Dallas|Houston|Minneapolis"},
    {"America/Winnipeg", "CST|CDT|Central Time|CT|Winnipeg|Manitoba"},
    {"America/Denver", "MST|MDT|Mountain Time|MT|Denver|Salt Lake City"},
    {"America/Edmonton", "MST|MDT|Mountain Time|MT|Edmonton|Calgary|Alberta"},
    {"America/Phoenix", "MST|Arizona|Phoenix|Scottsdale|Tucson"},
    {"America/Los_Angeles", "PST|PDT|Pacific Time|PT|California|San Francisco|SF|Los Angeles|LA|Seattle|Portland"},
    {"America/Vancouver", "PST|PDT|Pacific Time|PT|Vancouver|British Columbia"},
    {"America/Anchorage", "AKST|AKDT|Alaska Time|Alaska|Anchorage"},
    {"Pacific/Honolulu", "HST|Hawaii Time|Hawaii|Honolulu|Oahu"},
    {"America/Halifax", "AST|ADT|Atlantic Time|Halifax|Nova Scotia"},
    {"America/St_Johns", "NST|NDT|Newfoundland Time|Newfoundland|St Johns"},
    {"America/Mexico_City", "CST|CDT|Mexico City|CDMX"},
    {"America/Sao_Paulo", "BRT|Brasilia Time|Brazil|Sao Paulo|São Paulo"},

    {"Europe/London", "GMT|BST|British Time|UK|United Kingdom|London"},
    {"Europe/Dublin", "GMT|IST|Irish Time|Ireland|Dublin"},
    {"Europe/Zurich", "CET|CEST|Swiss Time|Switzerland|Zurich|Zürich"},
    {"Europe/Berlin", "CET|CEST|Germany|Berlin"},
    {"Europe/Paris", "CET|CEST|France|Paris"},
    {"Europe/Madrid", "CET|CEST|Spain|Madrid"},
    {"Europe/Rome", "CET|CEST|Italy|Rome"},
    {"Europe/Amsterdam", "CET|CEST|Netherlands|Amsterdam"},
    {"Europe/Prague", "CET|CEST|Czechia|Czech Republic|Prague"},
    {"Europe/Vienna", "CET|CEST|Austria|Vienna"},
    {"Europe/Warsaw", "CET|CEST|Poland|Warsaw"},
    {"Europe/Stockholm", "CET|CEST|Sweden|Stockholm"},
    {"Europe/Helsinki", "EET|EEST|Finland|Helsinki"},
    {"Europe/Athens", "EET|EEST|Greece|Athens"},
    {"Europe/Istanbul", "TRT|Turkey|Türkiye|Istanbul"},
    {"Europe/Moscow", "MSK|Moscow Time|Russia|Moscow"},

    {"Asia/Dubai", "GST|Gulf Time|UAE|United Arab Emirates|Dubai|Abu Dhabi"},
    {"Asia/Kolkata", "IST|India Time|Indian Standard Time|India|Delhi|Mumbai|Bangalore|Bengaluru"},
    {"Asia/Calcutta", "IST|India Time|Indian Standard Time|India|Delhi|Mumbai|Bangalore|Bengaluru|Kolkata"},
    {"Asia/Shanghai", "CST|China Time|China Standard Time|China|Beijing|Shanghai"},
    {"Asia/Hong_Kong", "HKT|Hong Kong"},
    {"Asia/Tokyo", "JST|Japan Time|Japan|Tokyo"},
    {"Asia/Seoul", "KST|Korea Time|Korea|Seoul"},
    {"Asia/Singapore", "SGT|Singapore Time|Singapore"},
    {"Asia/Bangkok", "ICT|Indochina Time|Thailand|Bangkok"},
    {"Asia/Jakarta", "WIB|Western Indonesia Time|Indonesia|Jakarta"},
    {"Asia/Manila", "PHT|Philippine Time|Philippines|Manila"},

    {"Australia/Sydney", "AEST|AEDT|Eastern Australia Time|Australia|Sydney"},
    {"Australia/Melbourne", "AEST|AEDT|Eastern Australia Time|Australia|Melbourne"},
    {"Australia/Brisbane", "AEST|Queensland|Australia|Brisbane"},
    {"Australia/Perth", "AWST|Western Australia Time|Australia|Perth"},
    {"Pacific/Auckland", "NZST|NZDT|New Zealand Time|New Zealand|Auckland"},
});

} // namespace

QStringList TimeZoneAliasCatalog::aliasesForTimeZoneId(const QString& timeZoneId)
{
    for (const TimeZoneAliasEntry& entry : kTimeZoneAliases) {
        if (timeZoneId == QString::fromUtf8(entry.timeZoneId)) {
            return QString::fromUtf8(entry.aliases).split(
                QLatin1Char('|'),
                Qt::SkipEmptyParts
            );
        }
    }

    return {};
}
