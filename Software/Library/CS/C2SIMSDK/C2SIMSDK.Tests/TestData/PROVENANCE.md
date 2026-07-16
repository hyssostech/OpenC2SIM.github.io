# Report test data - provenance

These fixtures are real VR-Forces C2SIM wire reports, captured off the STOMP bus
during a golden-trace run and used here to prove the SDK parses reports as they
actually arrive on the wire - not just schema-clean synthetic ones.

## Source

Captured from the running c2sim-server STOMP topic by the VRF_C2SIM interface and
recorded verbatim in:

    OpenC2SIM.github.io/Software/Interfaces/VRF_C2SIM/docs/golden-trace/reports-captured_wire-xml.log

Copied here on 2026-07-16 for the SDK 1.4.0 release work (register thread #21) so the
tests never reach outside this repository at runtime.

## Files

- `reports-captured_wire-xml.log` - verbatim copy of the full capture. Format per entry:
  a header line `[HH:MM:SS.mmm] REPORT #N (LLLL chars)` followed by a bare
  pretty-printed `<ReportBody>` element, entries separated by a blank line.
  Contents: 72 `ReportBody` messages / 144 `PositionReportContent` /
  120 empty `<OperationalStatusCode></OperationalStatusCode>` + 24 `FullyOperational`
  (= 144) / 120 empty `<StrengthPercentage></StrengthPercentage>`.

- `report-empty-status.xml` - REPORT #1 from the log. Both of its positions carry the
  empty `<OperationalStatusCode></OperationalStatusCode>` and empty
  `<StrengthPercentage></StrengthPercentage>` that strict `XmlSerializer` rejects
  (`'' is not a valid value for OperationalStatusCodeType`). Position data:
  `001aa71b-...` @ (58.703, 16.4992) and `04daa71b-...` @ (58.6755, 16.3907).

- `report-fully-operational.xml` - REPORT #3 from the log. Mixed on purpose: its first
  position has the empty status element, its second carries a genuine
  `<OperationalStatusCode>FullyOperational</OperationalStatusCode>` with
  `<StrengthPercentage>100</StrengthPercentage>` (SubjectEntity `2bdaa71b-...`). Proves
  a populated enum/value is preserved verbatim while an empty sibling is tolerated.

## Note on empty elements

An empty leaf element on the wire (`<OperationalStatusCode></OperationalStatusCode>`)
means "unspecified". The SDK's inbound deserialization now strips such empty leaves
before handing the XML to `XmlSerializer`; see `C2SIMSDK.ToC2SIMObject<T>`.
