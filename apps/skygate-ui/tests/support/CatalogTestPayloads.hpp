#pragma once

#include "CatalogArchiveTestSupport.hpp"

#include <QByteArray>

#include <string>

namespace skygate::ui::tests {

struct StarCatalogPayloadOptions final {
    int id = 1;
    int hip = 42;
    QByteArray properName = "Sirius";
    QByteArray ra = "6.7525";
    QByteArray dec = "-16.7161";
    QByteArray mag = "-1.46";
};

struct DeepSkyCatalogPayloadOptions final {
    QByteArray name = "NGC0224";
    QByteArray type = "G";
    QByteArray ra = "00:42:44.35";
    QByteArray dec = "+41:16:08.6";
    QByteArray constellation = "And";
    QByteArray messier = "31";
    QByteArray ngc = "0224";
    QByteArray identifiers = "PGC 2557,UGC 454";
    QByteArray commonName = "Andromeda Galaxy";
};

[[nodiscard]] inline QByteArray sampleHygCsvPayload(const StarCatalogPayloadOptions& options = {})
{
    QByteArray payload = "id,hip,proper,ra,dec,mag\n";
    payload += QByteArray::number(options.id);
    payload += ',';
    payload += QByteArray::number(options.hip);
    payload += ',';
    payload += options.properName;
    payload += ',';
    payload += options.ra;
    payload += ',';
    payload += options.dec;
    payload += ',';
    payload += options.mag;
    payload += '\n';
    return payload;
}

[[nodiscard]] inline QByteArray sampleOpenNgcCsvPayload(const DeepSkyCatalogPayloadOptions& options = {})
{
    QByteArray payload = "Name;Type;RA;Dec;Const;MajAx;MinAx;PosAng;B-Mag;V-Mag;J-Mag;H-Mag;K-Mag;"
                         "SurfBr;Hubble;Cstar U-Mag;Cstar B-Mag;Cstar V-Mag;M;NGC;IC;Cstar Names;"
                         "Identifiers;Common names;NED notes;OpenNGC notes\n";
    payload += options.name;
    payload += ';';
    payload += options.type;
    payload += ';';
    payload += options.ra;
    payload += ';';
    payload += options.dec;
    payload += ';';
    payload += options.constellation;
    payload += ";177.83;69.66;35;4.36;3.44;;;;13.35;SA(s)b;;;;";
    payload += options.messier;
    payload += ';';
    payload += options.ngc;
    payload += ";;;";
    payload += options.identifiers;
    payload += ';';
    payload += options.commonName;
    payload += ";;\n";
    return payload;
}

[[nodiscard]] inline QByteArray sampleCompactOpenNgcCsvPayload(const DeepSkyCatalogPayloadOptions& options = {})
{
    QByteArray payload = "Name;Type;RA;Dec;M;NGC;IC\n";
    payload += options.name;
    payload += ';';
    payload += options.type;
    payload += ';';
    payload += options.ra;
    payload += ';';
    payload += options.dec;
    payload += ';';
    payload += options.messier;
    payload += ';';
    payload += options.ngc;
    payload += ";\n";
    return payload;
}

[[nodiscard]] inline QByteArray sampleConstellationIndexJsonPayload()
{
    return R"({
        "constellations": [
            {
                "id": "orion",
                "common_name": { "english": "Orion" },
                "lines": [[24436, 25930, 26727]]
            }
        ]
    })";
}

// A ZIP archive with two supported catalog members. Without an explicit member
// selector the core selector reports the archive as ambiguous; selecting one
// member must load exactly that member and no object from the other one.
[[nodiscard]] inline QByteArray sampleTwoMemberCatalogZip()
{
    const std::string zipData = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec{
            .path = "catalog/hyg.csv",
            .data = sampleHygCsvPayload().toStdString(),
        },
        skygate::ephemeris::tests::ZipEntrySpec{
            .path = "catalog/ngc.csv",
            .data = sampleOpenNgcCsvPayload().toStdString(),
        },
    });
    return QByteArray(zipData.data(), static_cast<qsizetype>(zipData.size()));
}

}  // namespace skygate::ui::tests
