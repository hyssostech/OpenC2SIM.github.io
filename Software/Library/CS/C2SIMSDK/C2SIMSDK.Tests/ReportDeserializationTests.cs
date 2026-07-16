using System.Text;
using System.Text.RegularExpressions;
using C2SIM.Schema102;
using Xunit;

namespace C2SIM.Tests;

/// <summary>
/// Real VR-Forces wire reports carry empty leaf elements (notably
/// <c>&lt;OperationalStatusCode&gt;&lt;/OperationalStatusCode&gt;</c>) that the strict
/// <see cref="System.Xml.Serialization.XmlSerializer"/> rejects. The SDK's public parse path,
/// <see cref="C2SIMSDK.ToC2SIMObject{T}"/>, now sanitizes those away. These tests drive the
/// public path against the captured golden trace - see TestData/PROVENANCE.md.
/// </summary>
public sealed class ReportDeserializationTests
{
    static string TestDataDir => Path.Combine(AppContext.BaseDirectory, "TestData");
    static string Fixture(string name) => File.ReadAllText(Path.Combine(TestDataDir, name));

    /// <summary>
    /// The empty-enum report parses, and every scrap of position data survives the sanitization -
    /// spot-checked against the exact SubjectEntity UUIDs and coordinates in the fixture bytes.
    /// </summary>
    [Fact]
    public void Empty_status_report_parses_and_position_data_survives()
    {
        ReportBodyType report = C2SIMSDK.ToC2SIMObject<ReportBodyType>(Fixture("report-empty-status.xml"));

        Assert.NotNull(report);
        Assert.Equal(2, report.ReportContent.Length);

        PositionReportContentType p0 = Position(report, 0);
        Assert.Equal("001aa71b-4c26-a1ea-28b2-f7dfe8e76342", p0.SubjectEntity);
        Assert.Equal(58.703, Geo(p0).Latitude, 4);
        Assert.Equal(16.4992, Geo(p0).Longitude, 4);

        PositionReportContentType p1 = Position(report, 1);
        Assert.Equal("04daa71b-1777-a50c-03dd-d640fcdb6542", p1.SubjectEntity);
        Assert.Equal(58.6755, Geo(p1).Latitude, 4);
        Assert.Equal(16.3907, Geo(p1).Longitude, 4);
    }

    /// <summary>
    /// The mixed report's populated <c>FullyOperational</c> position keeps its status AND its
    /// StrengthPercentage of 100 - the strength discriminates a genuinely-read value from the
    /// enum default that an empty (now-stripped) element would leave behind.
    /// </summary>
    [Fact]
    public void Valid_status_report_yields_the_enum_value()
    {
        ReportBodyType report = C2SIMSDK.ToC2SIMObject<ReportBodyType>(Fixture("report-fully-operational.xml"));

        PositionReportContentType full = report.ReportContent
            .Select(rc => (PositionReportContentType)rc.Item)
            .Single(p => p.SubjectEntity == "2bdaa71b-6ae9-f248-462d-a82d9fd96342");

        OperationalStatusType status = full.EntityHealthStatus
            .Select(h => h.Item).OfType<OperationalStatusType>().Single();
        Assert.Equal(OperationalStatusCodeType.FullyOperational, status.OperationalStatusCode);

        StrengthType strength = full.EntityHealthStatus
            .Select(h => h.Item).OfType<StrengthType>().Single();
        Assert.Equal("100", strength.StrengthPercentage);
    }

    /// <summary>
    /// A populated, NON-default enum must pass through untouched - proves sanitization strips only
    /// empty leaves and never rewrites a value. (The golden trace only ever carries FullyOperational,
    /// which is enum index 0, so a non-default value has to be exercised synthetically.)
    /// </summary>
    [Fact]
    public void Populated_non_default_enum_survives_sanitization()
    {
        string xml =
            "<ReportBody xmlns=\"http://www.sisostds.org/schemas/C2SIM/1.1\">"
            + "<ReportContent><PositionReportContent>"
            + "<EntityHealthStatus><OperationalStatus>"
            + "<OperationalStatusCode>NotOperational</OperationalStatusCode>"
            + "</OperationalStatus></EntityHealthStatus>"
            + "<SubjectEntity>abc</SubjectEntity>"
            + "</PositionReportContent></ReportContent></ReportBody>";

        ReportBodyType report = C2SIMSDK.ToC2SIMObject<ReportBodyType>(xml);

        OperationalStatusType status = Position(report, 0).EntityHealthStatus
            .Select(h => h.Item).OfType<OperationalStatusType>().Single();
        Assert.Equal(OperationalStatusCodeType.NotOperational, status.OperationalStatusCode);
    }

    /// <summary>
    /// An object built with only the fields it needs (no empty optional elements) serializes and
    /// deserializes back unchanged - sanitization is a no-op on clean output and preserves values.
    /// </summary>
    [Fact]
    public void Round_trip_of_object_without_optional_fields_is_unchanged()
    {
        var report = new ReportBodyType
        {
            ReportContent = new[]
            {
                new ReportContentType
                {
                    Item = new PositionReportContentType
                    {
                        SubjectEntity = "1dfaa71b-8322-be55-ed8b-c050faa96542",
                        Location = new LocationType
                        {
                            Item = new GeodeticCoordinateType { Latitude = 58.6505, Longitude = 16.588 },
                        },
                    },
                },
            },
        };

        string xml1 = C2SIMSDK.FromC2SIMObject(report);
        ReportBodyType back = C2SIMSDK.ToC2SIMObject<ReportBodyType>(xml1);
        string xml2 = C2SIMSDK.FromC2SIMObject(back);

        // Sanitization removed nothing, so a re-serialization is byte-identical
        Assert.Equal(xml1, xml2);

        PositionReportContentType p = Position(back, 0);
        Assert.Equal("1dfaa71b-8322-be55-ed8b-c050faa96542", p.SubjectEntity);
        Assert.Equal(58.6505, Geo(p).Latitude, 4);
        Assert.Equal(16.588, Geo(p).Longitude, 4);
    }

    /// <summary>
    /// The whole captured trace parses through the public path: all 72 ReportBody messages
    /// deserialize, yielding 144 PositionReportContent, each with its SubjectEntity and coordinates.
    /// </summary>
    [Fact]
    public void All_golden_reports_parse_and_yield_every_position()
    {
        List<string> messages = SplitLog(Path.Combine(TestDataDir, "reports-captured_wire-xml.log"));
        Assert.Equal(72, messages.Count);

        int positions = 0;
        int parsed = 0;
        foreach (string msg in messages)
        {
            ReportBodyType report = C2SIMSDK.ToC2SIMObject<ReportBodyType>(msg);
            Assert.NotNull(report);
            parsed++;
            foreach (ReportContentType rc in report.ReportContent)
            {
                var p = (PositionReportContentType)rc.Item;
                Assert.False(string.IsNullOrWhiteSpace(p.SubjectEntity));
                GeodeticCoordinateType g = Geo(p);
                Assert.InRange(g.Latitude, -90.0, 90.0);
                Assert.InRange(g.Longitude, -180.0, 180.0);
                positions++;
            }
        }

        Assert.Equal(72, parsed);
        Assert.Equal(144, positions);
    }

    #region Helpers
    static PositionReportContentType Position(ReportBodyType r, int i) =>
        (PositionReportContentType)r.ReportContent[i].Item;

    static GeodeticCoordinateType Geo(PositionReportContentType p) =>
        (GeodeticCoordinateType)p.Location.Item;

    /// <summary>
    /// Split the capture into its individual bare <c>&lt;ReportBody&gt;</c> messages. Format:
    /// a <c>[HH:MM:SS.mmm] REPORT #N (LLLL chars)</c> header line, the pretty-printed XML (which
    /// has no internal blank lines), messages separated by a blank line.
    /// </summary>
    static List<string> SplitLog(string path)
    {
        var blocks = new List<string>();
        var sb = new StringBuilder();
        foreach (string line in File.ReadLines(path))
        {
            if (Regex.IsMatch(line, @"^\[\d{2}:\d{2}:\d{2}"))
            {
                if (sb.Length > 0) { blocks.Add(sb.ToString()); sb.Clear(); }
                continue;   // drop the header line
            }
            if (string.IsNullOrWhiteSpace(line)) { continue; }   // message separator
            sb.AppendLine(line);
        }
        if (sb.Length > 0) { blocks.Add(sb.ToString()); }
        return blocks;
    }
    #endregion
}
