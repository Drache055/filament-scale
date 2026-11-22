#include <unity.h>

#include <cstring>
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

void test_update_available() {
  TEST_ASSERT_TRUE(fwUpdateAvailable("v1.3", "v1.2"));
  TEST_ASSERT_FALSE(fwUpdateAvailable("v1.2", "v1.2"));
  TEST_ASSERT_FALSE(fwUpdateAvailable("v1.2", "v1.3"));
  TEST_ASSERT_FALSE(fwUpdateAvailable("v1.2", "v1.2-5-gabcdef"));  // Entwicklungsstand nach v1.2
  TEST_ASSERT_TRUE(fwUpdateAvailable("v1.2", "v1.2-beta"));
  TEST_ASSERT_TRUE(fwUpdateAvailable("v1.2", "dev"));  // eigene Version unbekannt
  TEST_ASSERT_FALSE(fwUpdateAvailable("v1.3-beta", "v1.2"));  // Pre-Releases nie anbieten
  TEST_ASSERT_FALSE(fwUpdateAvailable("", "v1.2"));
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

static const char *RELEASE =
    "{\"url\":\"https://api.github.com/repos/o/r/releases/1\",\"html_url\":\"https://github.com/o/r/releases/tag/v1.3\","
    "\"tag_name\":\"v1.3\",\"name\":\"v1.3\",\"draft\":false,\"prerelease\":false,"
    "\"author\":{\"login\":\"o\",\"id\":1},"
    "\"assets\":["
    "{\"name\":\"filament-scale-v1.3-factory.bin\",\"size\":2000000,\"uploader\":{\"login\":\"o\"},"
    "\"browser_download_url\":\"https://github.com/o/r/releases/download/v1.3/filament-scale-v1.3-factory.bin\"},"
    "{\"name\":\"filament-scale-v1.3-ota.bin\",\"size\":1900000,"
    "\"browser_download_url\":\"https://github.com/o/r/releases/download/v1.3/filament-scale-v1.3-ota.bin\"},"
    "{\"name\":\"filament-scale-v1.3.step\",\"size\":10,\"browser_download_url\":\"https://github.com/o/r/x.step\"}],"
    "\"body\":\"## What's Changed\\n* Fix \\u00e4 by @o\"}";

void test_parse_release() {
  FwRelease r;
  const char *err = nullptr;
  TEST_ASSERT_TRUE(fwParseRelease(RELEASE, strlen(RELEASE), r, err));
  TEST_ASSERT_EQUAL_STRING("v1.3", r.tag);
  TEST_ASSERT_EQUAL_STRING("https://github.com/o/r/releases/download/v1.3/filament-scale-v1.3-ota.bin", r.otaUrl);
  TEST_ASSERT_EQUAL_UINT32(1900000, r.otaSize);
  TEST_ASSERT_EQUAL_STRING("https://github.com/o/r/releases/tag/v1.3", r.pageUrl);
  TEST_ASSERT_EQUAL_STRING("## What's Changed\n* Fix \xC3\xA4 by @o", r.notes);
  TEST_ASSERT_FALSE(r.notesCut);
}

void test_parse_release_rejects() {
  FwRelease r;
  const char *err = nullptr;
  const char *bad = "{nope";
  TEST_ASSERT_FALSE(fwParseRelease(bad, strlen(bad), r, err));
  const char *pre = "{\"tag_name\":\"v1.3-beta\",\"prerelease\":false,\"assets\":[{\"name\":\"a-ota.bin\","
                    "\"browser_download_url\":\"https://github.com/a-ota.bin\"}]}";
  TEST_ASSERT_FALSE(fwParseRelease(pre, strlen(pre), r, err));
  const char *flagged = "{\"tag_name\":\"v1.3\",\"prerelease\":true,\"assets\":[{\"name\":\"a-ota.bin\","
                        "\"browser_download_url\":\"https://github.com/a-ota.bin\"}]}";
  TEST_ASSERT_FALSE(fwParseRelease(flagged, strlen(flagged), r, err));
  const char *noAsset = "{\"tag_name\":\"v1.3\",\"assets\":[{\"name\":\"a-factory.bin\","
                        "\"browser_download_url\":\"https://github.com/a-factory.bin\"}]}";
  TEST_ASSERT_FALSE(fwParseRelease(noAsset, strlen(noAsset), r, err));
  const char *foreign = "{\"tag_name\":\"v1.3\",\"assets\":[{\"name\":\"a-ota.bin\","
                        "\"browser_download_url\":\"https://evil.example/a-ota.bin\"}]}";
  TEST_ASSERT_FALSE(fwParseRelease(foreign, strlen(foreign), r, err));
  TEST_ASSERT_NOT_NULL(err);
}

void test_notes_cut_at_utf8_boundary() {
  std::string body(FW_NOTES_LEN - 1, 'a');
  body += "\xC3\xA4\xC3\xA4";  // "ää": das erste ä beginnt beim letzten erlaubten Byte
  std::string json = "{\"tag_name\":\"v1.3\",\"assets\":[{\"name\":\"a-ota.bin\","
                     "\"browser_download_url\":\"https://github.com/a-ota.bin\"}],\"body\":\"" + body + "\"}";
  FwRelease r;
  const char *err = nullptr;
  TEST_ASSERT_TRUE(fwParseRelease(json.c_str(), json.size(), r, err));
  TEST_ASSERT_TRUE(r.notesCut);
  TEST_ASSERT_EQUAL(FW_NOTES_LEN - 1, strlen(r.notes));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_versions);
  RUN_TEST(test_compare);
  RUN_TEST(test_update_available);
  RUN_TEST(test_allowed_url);
  RUN_TEST(test_parse_release);
  RUN_TEST(test_parse_release_rejects);
  RUN_TEST(test_notes_cut_at_utf8_boundary);
  return UNITY_END();
}
