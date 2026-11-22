#include <unity.h>

#include <cstring>
#include <sstream>
#include <string>

#include "core/update_core.h"

void setUp() {}
void tearDown() {}

void test_parse_versions() {
  FwVersion v = fwParseVersion("v1.2");
  TEST_ASSERT_TRUE(v.valid);
  TEST_ASSERT_EQUAL(1, v.major);
  TEST_ASSERT_EQUAL(2, v.minor);
  TEST_ASSERT_EQUAL(0, v.patch);
  TEST_ASSERT_FALSE(v.prerelease);
  TEST_ASSERT_EQUAL(0, v.ahead);

  v = fwParseVersion("1.10.3");
  TEST_ASSERT_TRUE(v.valid);
  TEST_ASSERT_EQUAL(10, v.minor);
  TEST_ASSERT_EQUAL(3, v.patch);

  v = fwParseVersion("v1.2-beta");
  TEST_ASSERT_TRUE(v.valid);
  TEST_ASSERT_TRUE(v.prerelease);

  v = fwParseVersion("v1.2-3-gabc1234");
  TEST_ASSERT_TRUE(v.valid);
  TEST_ASSERT_FALSE(v.prerelease);
  TEST_ASSERT_EQUAL(3, v.ahead);

  v = fwParseVersion("v1.2-beta-4-gabc1234-dirty");
  TEST_ASSERT_TRUE(v.valid);
  TEST_ASSERT_TRUE(v.prerelease);
  TEST_ASSERT_EQUAL(4, v.ahead);

  v = fwParseVersion("v1.2-dirty");
  TEST_ASSERT_TRUE(v.valid);
  TEST_ASSERT_FALSE(v.prerelease);
  TEST_ASSERT_EQUAL(1, v.ahead);

  TEST_ASSERT_FALSE(fwParseVersion("dev").valid);
  TEST_ASSERT_FALSE(fwParseVersion("ae7f68f").valid);
  TEST_ASSERT_FALSE(fwParseVersion("v1").valid);
  TEST_ASSERT_FALSE(fwParseVersion("v1.2x").valid);
  TEST_ASSERT_FALSE(fwParseVersion("").valid);
  TEST_ASSERT_FALSE(fwParseVersion(nullptr).valid);
}

static int cmp(const char *a, const char *b) { return fwCompareVersions(fwParseVersion(a), fwParseVersion(b)); }

void test_compare() {
  TEST_ASSERT_TRUE(cmp("v1.2", "v1.10") < 0);  // numerisch, nicht als Text
  TEST_ASSERT_TRUE(cmp("v2.0", "v1.9.9") > 0);
  TEST_ASSERT_TRUE(cmp("v1.2.1", "v1.2") > 0);
  TEST_ASSERT_EQUAL(0, cmp("v1.2", "1.2.0"));
  TEST_ASSERT_TRUE(cmp("v1.2-beta", "v1.2") < 0);
  TEST_ASSERT_TRUE(cmp("v1.2-beta", "v1.1") > 0);
  TEST_ASSERT_TRUE(cmp("v1.2-3-gabc", "v1.2") > 0);
  TEST_ASSERT_TRUE(cmp("v1.2-3-gabc", "v1.3") < 0);
  TEST_ASSERT_TRUE(cmp("dev", "v0.1") < 0);
}

void test_prerelease_order() {
  FwVersion v = fwParseVersion("v1.2-rc2");
  TEST_ASSERT_TRUE(v.prerelease);
  TEST_ASSERT_TRUE(v.rc);
  TEST_ASSERT_EQUAL(2, v.preNum);
  v = fwParseVersion("v1.2-beta.3");
  TEST_ASSERT_FALSE(v.rc);
  TEST_ASSERT_EQUAL(3, v.preNum);
  TEST_ASSERT_TRUE(cmp("v1.2-beta2", "v1.2-beta10") < 0);
  TEST_ASSERT_TRUE(cmp("v1.2-test9", "v1.2-rc1") < 0);
  TEST_ASSERT_TRUE(cmp("v1.2-rc1", "v1.2-rc2") < 0);
  TEST_ASSERT_TRUE(cmp("v1.2-rc9", "v1.2") < 0);
  TEST_ASSERT_TRUE(cmp("v1.3-beta1", "v1.2") > 0);
}

void test_channels() {
  TEST_ASSERT_EQUAL(FwChannel::Stable, fwReleaseChannel("v1.2", false));
  TEST_ASSERT_EQUAL(FwChannel::Rc, fwReleaseChannel("v1.2", true));
  TEST_ASSERT_EQUAL(FwChannel::Rc, fwReleaseChannel("v1.2-rc1", true));
  TEST_ASSERT_EQUAL(FwChannel::Rc, fwReleaseChannel("v1.2-RC1", false));
  TEST_ASSERT_EQUAL(FwChannel::Test, fwReleaseChannel("v1.2-beta", true));
  TEST_ASSERT_EQUAL(FwChannel::Test, fwReleaseChannel("v1.2-test3", true));
  FwChannel ch;
  TEST_ASSERT_TRUE(fwChannelFromName("rc", ch));
  TEST_ASSERT_EQUAL(FwChannel::Rc, ch);
  TEST_ASSERT_FALSE(fwChannelFromName("beta", ch));
  TEST_ASSERT_EQUAL_STRING("test", fwChannelName(FwChannel::Test));
}

void test_update_available() {
  TEST_ASSERT_TRUE(fwUpdateAvailable("v1.3", "v1.2"));
  TEST_ASSERT_FALSE(fwUpdateAvailable("v1.2", "v1.2"));
  TEST_ASSERT_FALSE(fwUpdateAvailable("v1.2", "v1.3"));
  TEST_ASSERT_FALSE(fwUpdateAvailable("v1.2", "v1.2-5-gabcdef"));  // Entwicklungsstand nach v1.2
  TEST_ASSERT_TRUE(fwUpdateAvailable("v1.2", "v1.2-rc1"));
  TEST_ASSERT_TRUE(fwUpdateAvailable("v1.3-rc1", "v1.2"));
  TEST_ASSERT_TRUE(fwUpdateAvailable("v1.2", "dev"));  // eigene Version unbekannt
  TEST_ASSERT_FALSE(fwUpdateAvailable("", "v1.2"));
  TEST_ASSERT_TRUE(fwIsDowngrade("v1.2", "v1.3-beta1"));  // zurück von Test auf Stable
  TEST_ASSERT_FALSE(fwIsDowngrade("v1.3", "v1.2"));
  TEST_ASSERT_FALSE(fwIsDowngrade("v1.2", "dev"));
}

void test_allowed_url() {
  TEST_ASSERT_TRUE(fwAllowedUrl("https://github.com/Drache055/filament-scale/releases/download/v1.2/x-ota.bin"));
  TEST_ASSERT_TRUE(fwAllowedUrl("https://release-assets.githubusercontent.com/github-production-release-asset/1?sig=x"));
  TEST_ASSERT_TRUE(fwAllowedUrl("https://objects.githubusercontent.com/x"));
  TEST_ASSERT_FALSE(fwAllowedUrl("http://github.com/x"));
  TEST_ASSERT_FALSE(fwAllowedUrl("https://github.com.evil.example/x"));
  TEST_ASSERT_FALSE(fwAllowedUrl("https://githubusercontent.com/x"));
  TEST_ASSERT_FALSE(fwAllowedUrl("https://evilgithubusercontent.com/x"));
  TEST_ASSERT_FALSE(fwAllowedUrl("https://evil.example/github.com"));
  TEST_ASSERT_FALSE(fwAllowedUrl(nullptr));
}

// Liste wie von GET /releases (neueste zuerst)
static const char *RELEASES =
    "[{\"tag_name\":\"v1.4-beta1\",\"draft\":false,\"prerelease\":true,\"html_url\":\"https://github.com/o/r/releases/tag/v1.4-beta1\","
    "\"body\":\"beta\",\"assets\":[{\"name\":\"filament-scale-v1.4-beta1-ota.bin\",\"size\":11,"
    "\"browser_download_url\":\"https://github.com/o/r/releases/download/v1.4-beta1/filament-scale-v1.4-beta1-ota.bin\"}]},"
    "{\"tag_name\":\"v1.5\",\"draft\":true,\"prerelease\":false,\"assets\":[{\"name\":\"x-ota.bin\",\"size\":1,"
    "\"browser_download_url\":\"https://github.com/o/r/x-ota.bin\"}]},"
    "{\"tag_name\":\"v1.3-rc2\",\"draft\":false,\"prerelease\":true,\"html_url\":\"https://github.com/o/r/releases/tag/v1.3-rc2\","
    "\"body\":\"rc\",\"assets\":[{\"name\":\"filament-scale-v1.3-rc2-ota.bin\",\"size\":22,"
    "\"browser_download_url\":\"https://github.com/o/r/releases/download/v1.3-rc2/filament-scale-v1.3-rc2-ota.bin\"}]},"
    "{\"tag_name\":\"v1.3-rc3\",\"draft\":false,\"prerelease\":true,\"body\":\"no ota\",\"assets\":[]},"
    "{\"tag_name\":\"nightly\",\"draft\":false,\"prerelease\":true,\"assets\":[{\"name\":\"x-ota.bin\",\"size\":1,"
    "\"browser_download_url\":\"https://github.com/o/r/x-ota.bin\"}]},"
    "{\"tag_name\":\"v1.2\",\"draft\":false,\"prerelease\":false,\"html_url\":\"https://github.com/o/r/releases/tag/v1.2\","
    "\"author\":{\"login\":\"o\"},\"body\":\"## What's Changed\\n* Fix \\u00e4 by @o\",\"assets\":["
    "{\"name\":\"filament-scale-v1.2-factory.bin\",\"size\":2000000,"
    "\"browser_download_url\":\"https://github.com/o/r/releases/download/v1.2/filament-scale-v1.2-factory.bin\"},"
    "{\"name\":\"filament-scale-v1.2-ota.bin\",\"size\":1900000,\"uploader\":{\"login\":\"o\"},"
    "\"browser_download_url\":\"https://github.com/o/r/releases/download/v1.2/filament-scale-v1.2-ota.bin\"}]},"
    "{\"tag_name\":\"v1.1\",\"draft\":false,\"prerelease\":false,\"assets\":[{\"name\":\"a-ota.bin\",\"size\":1,"
    "\"browser_download_url\":\"https://github.com/o/r/a-ota.bin\"}]}]";

void test_pick_per_channel() {
  FwRelease r;
  bool found = false;
  const char *err = nullptr;
  size_t n = strlen(RELEASES);
  TEST_ASSERT_TRUE(fwParseReleases(RELEASES, n, FwChannel::Stable, r, found, err));
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_EQUAL_STRING("v1.2", r.tag);
  TEST_ASSERT_EQUAL(FwChannel::Stable, r.channel);
  TEST_ASSERT_EQUAL_STRING("https://github.com/o/r/releases/download/v1.2/filament-scale-v1.2-ota.bin", r.otaUrl);
  TEST_ASSERT_EQUAL_UINT32(1900000, r.otaSize);
  TEST_ASSERT_EQUAL_STRING("https://github.com/o/r/releases/tag/v1.2", r.pageUrl);
  TEST_ASSERT_EQUAL_STRING("## What's Changed\n* Fix \xC3\xA4 by @o", r.notes);
  TEST_ASSERT_FALSE(r.notesCut);

  TEST_ASSERT_TRUE(fwParseReleases(RELEASES, n, FwChannel::Rc, r, found, err));
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_EQUAL_STRING("v1.3-rc2", r.tag);  // v1.3-rc3 hat kein OTA-Image
  TEST_ASSERT_EQUAL(FwChannel::Rc, r.channel);
  TEST_ASSERT_EQUAL_UINT32(22, r.otaSize);

  TEST_ASSERT_TRUE(fwParseReleases(RELEASES, n, FwChannel::Test, r, found, err));
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_EQUAL_STRING("v1.4-beta1", r.tag);  // Entwurf v1.5 zählt nicht
  TEST_ASSERT_EQUAL(FwChannel::Test, r.channel);
  TEST_ASSERT_EQUAL_STRING("beta", r.notes);
}

void test_pick_from_stream() {
  FwRelease r;
  bool found = false;
  const char *err = nullptr;
  std::istringstream in(RELEASES);
  TEST_ASSERT_TRUE(fwParseReleases(in, FwChannel::Stable, r, found, err));
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_EQUAL_STRING("v1.2", r.tag);
  TEST_ASSERT_EQUAL_UINT32(1900000, r.otaSize);
  std::istringstream bad("{not json");
  TEST_ASSERT_FALSE(fwParseReleases(bad, FwChannel::Stable, r, found, err));
}

void test_parse_releases_edge_cases() {
  FwRelease r;
  bool found = true;
  const char *err = nullptr;
  const char *empty = "[]";
  TEST_ASSERT_TRUE(fwParseReleases(empty, strlen(empty), FwChannel::Test, r, found, err));
  TEST_ASSERT_FALSE(found);
  const char *onlyPre = "[{\"tag_name\":\"v0.1\",\"prerelease\":true,\"assets\":[{\"name\":\"a-ota.bin\","
                        "\"browser_download_url\":\"https://github.com/a-ota.bin\"}]}]";
  TEST_ASSERT_TRUE(fwParseReleases(onlyPre, strlen(onlyPre), FwChannel::Stable, r, found, err));
  TEST_ASSERT_FALSE(found);  // v0.1 ist auf GitHub als Pre-Release markiert
  TEST_ASSERT_TRUE(fwParseReleases(onlyPre, strlen(onlyPre), FwChannel::Rc, r, found, err));
  TEST_ASSERT_TRUE(found);
  const char *foreign = "[{\"tag_name\":\"v1.3\",\"assets\":[{\"name\":\"a-ota.bin\","
                        "\"browser_download_url\":\"https://evil.example/a-ota.bin\"}]}]";
  TEST_ASSERT_TRUE(fwParseReleases(foreign, strlen(foreign), FwChannel::Test, r, found, err));
  TEST_ASSERT_FALSE(found);
  const char *bad = "{nope";
  TEST_ASSERT_FALSE(fwParseReleases(bad, strlen(bad), FwChannel::Stable, r, found, err));
  TEST_ASSERT_NOT_NULL(err);
  const char *obj = "{\"message\":\"Not Found\"}";
  TEST_ASSERT_FALSE(fwParseReleases(obj, strlen(obj), FwChannel::Stable, r, found, err));
}

void test_notes_cut_at_utf8_boundary() {
  std::string body(FW_NOTES_LEN - 1, 'a');
  body += "\xC3\xA4\xC3\xA4";  // "ää": das erste ä beginnt beim letzten erlaubten Byte
  std::string json = "[{\"tag_name\":\"v1.3\",\"assets\":[{\"name\":\"a-ota.bin\","
                     "\"browser_download_url\":\"https://github.com/a-ota.bin\"}],\"body\":\"" + body + "\"}]";
  FwRelease r;
  bool found = false;
  const char *err = nullptr;
  TEST_ASSERT_TRUE(fwParseReleases(json.c_str(), json.size(), FwChannel::Stable, r, found, err));
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_TRUE(r.notesCut);
  TEST_ASSERT_EQUAL(FW_NOTES_LEN - 1, strlen(r.notes));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_versions);
  RUN_TEST(test_compare);
  RUN_TEST(test_prerelease_order);
  RUN_TEST(test_channels);
  RUN_TEST(test_update_available);
  RUN_TEST(test_allowed_url);
  RUN_TEST(test_pick_per_channel);
  RUN_TEST(test_pick_from_stream);
  RUN_TEST(test_parse_releases_edge_cases);
  RUN_TEST(test_notes_cut_at_utf8_boundary);
  return UNITY_END();
}
