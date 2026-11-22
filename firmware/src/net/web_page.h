#pragma once

// Weboberfläche (eine Seite, komplett eingebettet), zweisprachig (Deutsch/Englisch, folgt der Gerätesprache).
// Unterseiten per #hash, Daten per Polling von /api/status, /api/spoolman und /api/log.
static const char WEB_PAGE[] = R"HTML(<!doctype html>
<html lang="de"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Filamentwaage</title>
<style>
:root{--bg:#0d100f;--surface:rgba(255,255,255,.04);--ink:#f6f6f6;--muted:rgba(246,246,246,.62);--line:rgba(255,255,255,.12);--accent:#7aa7ff;--accent-ink:#0b1220;--pill:rgba(255,255,255,.08);--warn:#f0a43c;--ok:#4cd08a;--err:#ff6b6b;--screen:#05070b;--glow1:rgba(80,120,255,.28);--glow2:rgba(190,110,255,.18);--r:14px;--card:#161a19;--dim:var(--muted);--text:var(--ink);--pri:var(--accent);--side:var(--bg);color-scheme:dark}
@media (prefers-color-scheme:light){:root{--bg:#f6f5f0;--surface:#fff;--ink:#0d100f;--muted:rgba(13,16,15,.62);--line:rgba(13,16,15,.12);--accent:#2457d6;--accent-ink:#fff;--pill:rgba(13,16,15,.06);--warn:#b45309;--ok:#15803d;--err:#c62828;--glow1:rgba(80,120,255,.22);--glow2:rgba(190,110,255,.14);--card:#fff;color-scheme:light}}
*{box-sizing:border-box}
body{margin:0;min-height:100vh;color:var(--ink);font:16px/1.55 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:var(--bg) radial-gradient(60% 40% at 75% 0,var(--glow1),transparent 70%) no-repeat,radial-gradient(45% 35% at 10% 15%,var(--glow2),transparent 70%) no-repeat;background-attachment:fixed}
:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.app{display:block}
aside{position:sticky;top:0;z-index:5;background:var(--bg);border-bottom:1px solid var(--line);padding:10px 20px}
.brand{font-weight:700;font-stretch:112%;font-size:17px;display:flex;align-items:center;gap:10px;margin-bottom:8px}
#dot{width:9px;height:9px;border-radius:50%;background:var(--err)}
#dot.on{background:var(--ok);box-shadow:0 0 8px var(--ok)}
#menu{display:flex;gap:4px;overflow-x:auto;padding-bottom:2px}
#menu a{color:var(--muted);text-decoration:none;padding:7px 14px;border-radius:999px;font-size:14px;white-space:nowrap;transition:background .2s,color .2s}
#menu a:hover{background:var(--pill);color:var(--ink)}
#menu a.active{background:var(--accent);color:var(--accent-ink);font-weight:600}
main{padding:28px 20px 60px;max-width:960px;width:100%;margin:0 auto}
.top{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:22px;flex-wrap:wrap}
h1{font-size:clamp(30px,5vw,46px);font-weight:700;font-stretch:112%;line-height:1.1;letter-spacing:-.02em;margin:0;text-wrap:balance}
#mini{color:var(--muted);font:500 14px ui-monospace,Menlo,Consolas,monospace;font-variant-numeric:tabular-nums}
.page{display:none}.page.active{display:block}
.card{background:var(--surface);border:1px solid var(--line);border-radius:var(--r);padding:18px 20px;margin-bottom:14px;min-width:0}
.label{display:inline-block;font:500 12px ui-monospace,Menlo,Consolas,monospace;text-transform:uppercase;letter-spacing:.09em;color:var(--muted);background:var(--pill);padding:4px 12px;border-radius:999px;margin-bottom:12px}
.val{font-size:18px;font-weight:600;word-break:break-word}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:14px;margin-bottom:14px}
.grid .card{margin:0}.grid .wide{grid-column:1/-1}
#weight{font-size:clamp(56px,12vw,96px);font-weight:700;font-stretch:112%;letter-spacing:-.03em;font-variant-numeric:tabular-nums;line-height:1.05}
#state{min-height:1.4em;color:var(--muted);margin-bottom:8px}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
button{background:var(--accent);color:var(--accent-ink);border:0;border-radius:999px;padding:10px 20px;font:inherit;font-weight:600;font-size:15px;cursor:pointer;transition:filter .2s,background .2s}
button:hover{filter:brightness(1.1)}
button.sec{background:var(--pill);color:var(--ink);font-weight:500}
button.sec:hover{filter:none;background:var(--line)}
input,textarea,select{background:var(--surface);color:var(--ink);border:1px solid var(--line);border-radius:var(--r);padding:9px 12px;font:inherit;max-width:100%}
input:focus,textarea:focus,select:focus{outline:2px solid var(--accent);outline-offset:0;border-color:transparent}
input[type=checkbox],input[type=radio]{accent-color:var(--accent);width:1.1em;height:1.1em;padding:0}
select option{color:#000}
input[type=number]{width:7em}
input[type=color]{width:3em;padding:2px;height:2.6em;flex:none}
textarea{width:100%;height:300px;font:13px/1.45 ui-monospace,Menlo,Consolas,monospace}
table{width:100%;border-collapse:collapse}
td{padding:9px 0;border-bottom:1px solid var(--line)}tr:last-child td{border-bottom:0}
td:last-child{text-align:right;font-variant-numeric:tabular-nums}
p.hint{color:var(--muted);margin:0 0 12px;font-size:15px}
ol.steps{margin:0 0 14px;padding-left:20px;color:var(--muted)}ol.steps li{margin-bottom:4px}
.item{display:flex;gap:8px;align-items:center;margin-bottom:8px;flex-wrap:wrap}
.item .name{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis}
.item input[type=text],.item input:not([type]){flex:1;min-width:0}.item input.n{flex:none;width:5em}
.item button{padding:8px 14px}
.item select{padding:8px 6px;max-width:9em}
.item .tarebox{display:flex;gap:4px;align-items:center;font-size:.9em}
body:not([data-role=admin]) .admin-only{display:none!important}
body:not([data-role=user]) .user-only{display:none!important}
body:not([data-role=admin]):not([data-cat="1"]) .cat-only{display:none!important}
#login{position:fixed;inset:0;display:none;align-items:center;justify-content:center;padding:16px;background:var(--bg) radial-gradient(60% 50% at 70% 10%,var(--glow1),transparent 70%) no-repeat;z-index:20}
#login .box{width:100%;max-width:380px;padding:24px}
#login h1{margin-bottom:16px;font-size:32px}
#login input{width:100%;margin-bottom:10px}
#login .row button{flex:1}
#whoami{color:var(--muted);margin-right:6px}
.net{cursor:pointer}.net:hover{background:var(--pill)}
h3{font-size:15px;margin:18px 0 8px}
#logbox{background:var(--screen);color:#cbd5e1;border:1px solid var(--line);border-radius:var(--r);padding:12px;height:62vh;overflow:auto;font:12px/1.45 ui-monospace,Menlo,Consolas,monospace;white-space:pre-wrap;word-break:break-all;margin:0}
.ok{color:var(--ok)}.warn{color:var(--warn)}.err{color:var(--err)}
#fwbar{width:100%;height:10px;margin-top:12px;accent-color:var(--accent)}
#fwnotes{background:var(--screen);color:#cbd5e1;border:1px solid var(--line);border-radius:var(--r);padding:12px;max-height:40vh;overflow:auto;font:13px/1.45 ui-monospace,Menlo,Consolas,monospace;white-space:pre-wrap;word-break:break-word;margin:12px 0 0}
#menu a.upd::after{content:'';display:inline-block;width:7px;height:7px;border-radius:50%;background:var(--accent);margin-left:6px;vertical-align:middle}
#toast{position:fixed;left:50%;bottom:22px;transform:translateX(-50%) translateY(30px);background:var(--card);border:1px solid var(--line);border-radius:999px;padding:12px 22px;box-shadow:0 8px 30px #0006;opacity:0;pointer-events:none;transition:.25s;max-width:92vw;z-index:9}
#toast.show{opacity:1;transform:translateX(-50%)}
@media (max-width:760px){
  aside{padding:8px 14px}
  main{padding:20px 14px 50px}
  #logbox{height:55vh}
}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
</style></head><body>
<div id="login"><div class="card box">
  <h1 data-i18n="login_title">Anmelden</h1>
  <input id="lname" data-i18n-ph="login_name" autocomplete="username" autocapitalize="none" maxlength="23">
  <input id="lsecret" type="password" data-i18n-ph="login_secret" autocomplete="current-password" maxlength="32">
  <div class="row"><button id="lgo" data-i18n="login">Anmelden</button><button id="lgen" class="sec" style="display:none" data-i18n="generic_login">Generic (nur ansehen)</button></div>
  <div id="lerr" class="err" style="min-height:1.4em;margin-top:10px"></div>
</div></div>
<div class="app" id="wrap" style="display:none">
<aside>
  <div class="brand"><span id="dot"></span>Filamentwaage</div>
  <nav id="menu">
    <a href="#overview" data-page="overview" data-i18n="m_overview">Übersicht</a>
    <a class="admin-only" href="#calibration" data-page="calibration" data-i18n="m_calibration">Kalibrierung</a>
    <a class="admin-only" href="#users" data-page="users" data-i18n="m_users">Benutzer</a>
    <a class="cat-only" href="#catalog" data-page="catalog" data-i18n="m_catalog">Filament-Katalog</a>
    <a class="admin-only" href="#spoolman" data-page="spoolman" data-i18n="m_spoolman">Spoolman</a>
    <a class="admin-only" href="#wifi" data-page="wifi" data-i18n="m_wifi">WLAN</a>
    <a class="admin-only" href="#log" data-page="log" data-i18n="m_log">Log</a>
    <a class="admin-only" href="#system" data-page="system" data-i18n="m_system">System</a>
  </nav>
</aside>
<main>
<div class="top"><h1 id="title">Übersicht</h1><div class="row"><span id="whoami"></span><span id="mini"></span><button id="logout" class="sec" data-i18n="logout">Abmelden</button></div></div>

<!-- Übersicht -->
<section class="page" id="p-overview">
  <div class="grid">
    <div class="card wide">
      <div class="label" data-i18n="weight">Gewicht</div>
      <div id="weight">-- g</div>
      <div id="state"></div>
      <div class="row"><button id="tare" style="display:none" data-i18n="tare">Tara</button></div>
    </div>
    <div class="card"><div class="label" data-i18n="cur_user">Angemeldeter Benutzer</div><div class="val" id="curuser">-</div><div style="color:var(--dim);margin-top:4px;font-size:.9em" id="devuser"></div></div>
    <div class="card"><div class="label">WLAN</div><div class="val" id="wifishort">-</div></div>
    <div class="card admin-only"><div class="label">Spoolman</div><div class="val" id="smshort">-</div></div>
    <div class="card"><div class="label">NFC</div><div class="val" id="nfcshort">-</div></div>
  </div>
  <div class="card admin-only"><div class="label" data-i18n="last_scan">Zuletzt gescannte Spule</div><div id="lastscan1">-</div></div>
  <div class="card user-only">
    <div class="label" data-i18n="my_settings">Meine Einstellungen</div>
    <div class="row" style="margin-bottom:12px"><span data-i18n="my_lang">Meine Sprache</span><select id="mylang"><option value="" data-i18n="lang_device">Gerätesprache</option><option value="de">Deutsch</option><option value="en">English</option></select></div>
    <div class="row"><input id="oldpin" type="password" inputmode="numeric" maxlength="6" data-i18n-ph="old_pin" style="width:8em" autocomplete="off"><input id="newpin2" type="password" inputmode="numeric" maxlength="6" data-i18n-ph="new_pin" style="width:8em" autocomplete="off"><button id="changepin" data-i18n="change_pin">PIN ändern</button></div>
    <p class="hint" style="margin:8px 0 0" data-i18n="pin_rules"></p>
  </div>
  <div class="card admin-only"><div class="label" data-i18n="device">Gerät</div>
    <table>
      <tr><td data-i18n="wlan">WLAN</td><td id="ssid">-</td></tr>
      <tr><td data-i18n="ip">IP-Adresse</td><td id="ip">-</td></tr>
      <tr><td data-i18n="signal">Signal</td><td id="rssi">-</td></tr>
      <tr><td data-i18n="uptime">Laufzeit</td><td id="up">-</td></tr>
      <tr><td data-i18n="mem">Freier RAM / PSRAM</td><td id="heap">-</td></tr>
      <tr><td>Firmware</td><td id="fwver">-</td></tr>
    </table>
  </div>
</section>

<!-- Kalibrierung -->
<section class="page" id="p-calibration">
  <div class="card">
    <div class="label" data-i18n="status">Status</div>
    <div class="val" id="calstate">-</div>
    <div style="color:var(--dim);margin-top:6px"><span data-i18n="raw_net">Rohwert (netto)</span>: <span id="rawnet">-</span></div>
  </div>
  <div class="card">
    <div class="label" data-i18n="procedure">Ablauf</div>
    <ol class="steps"><li data-i18n-html="cal1"></li><li data-i18n-html="cal2"></li><li data-i18n-html="cal3"></li></ol>
    <div class="row"><button class="sec" id="tare2" data-i18n="tare">Tara</button>
      <input id="ref" type="number" min="1" max="5000" step="1" value="200"> <span>g</span>
      <button id="cal" data-i18n="calibrate">Kalibrieren</button></div>
  </div>
  <div class="card">
    <div class="label" data-i18n="sensor">Wägesensor</div>
    <div class="row"><select id="stype"><option value="hx711">HX711 / HX710B</option><option value="nau7802">NAU7802 (I²C)</option><option value="ads1232">ADS1232 / ADS1231</option></select></div>
    <div class="row" id="spins" style="margin-top:8px"></div>
    <p style="color:var(--dim)" id="shint"></p>
    <p style="color:var(--dim)" data-i18n="sensor_cal_hint"></p>
    <p class="warn" id="srestart" style="display:none" data-i18n="sensor_restart"></p>
    <div class="row"><button id="ssave" data-i18n="sensor_save">Speichern und neu starten</button></div>
  </div>
</section>

<!-- Benutzer -->
<section class="page" id="p-users">
  <div class="card" id="admincard">
    <div class="label" data-i18n="admin">Admin</div>
    <p class="hint" data-i18n="admin_hint"></p>
    <p class="warn" id="admwarn" style="display:none" data-i18n="admin_default_warn"></p>
    <p class="warn" id="admpinwarn" style="display:none" data-i18n="dev_pin_warn"></p>
    <label class="row"><input type="checkbox" id="admlock"><span data-i18n="admin_lock">Admin-Sperre aktiv</span></label>
    <div class="row" style="margin-top:8px"><input id="admpw" type="password" data-i18n-ph="new_password" maxlength="32" style="width:14em" autocomplete="new-password"><button id="admsetpw" data-i18n="change_password">Passwort ändern</button></div>
    <div class="row" style="margin-top:8px"><input id="admdpin" type="password" inputmode="numeric" maxlength="6" data-i18n-ph="dev_pin" style="width:14em" autocomplete="off"><button id="admsetdpin" data-i18n="set_dev_pin">Geräte-PIN setzen</button></div>
    <div class="row" style="margin-top:8px"><button id="admtag" data-i18n="write_admin_tag">Admin-Tag schreiben</button></div>
    <div class="label" style="margin-top:16px" data-i18n="generic">Generic-Zugang</div>
    <label class="row"><input type="checkbox" id="genon"><span data-i18n="generic_on">Generic-Zugang aktiv (nur wiegen und NFC lesen)</span></label>
    <div class="row" style="margin-top:8px"><input id="genpin" type="password" inputmode="numeric" maxlength="6" data-i18n-ph="pin_optional" style="width:10em" autocomplete="off"><button id="gensetpin" class="sec" data-i18n="set_pin">PIN setzen</button></div>
  </div>
  <div class="card">
    <p class="hint" data-i18n="users_hint"></p>
    <div class="row"><input id="newuser" data-i18n-ph="name" maxlength="23" style="width:12em"><input id="newpin" type="password" inputmode="numeric" maxlength="6" data-i18n-ph="pin_optional" style="width:9em" autocomplete="off"><button id="adduser" data-i18n="create">Anlegen</button></div>
  </div>
  <div class="card"><div class="label" data-i18n="m_users">Benutzer</div><div id="users"></div><div id="writemsg" style="min-height:1.4em;margin-top:8px"></div></div>
</section>

<!-- Filament-Katalog -->
<section class="page" id="p-catalog">
  <div class="card">
    <div class="label" data-i18n="sync_title">Abgleich mit Spoolman</div>
    <p class="hint" data-i18n="sync_hint"></p>
    <div class="row"><button id="syncsm" class="sec" data-i18n="sync_now">Jetzt abgleichen</button><span id="syncinfo" style="color:var(--dim)"></span></div>
  </div>
  <div class="card">
    <p class="hint" data-i18n-html="cat_hint"></p>
    <div class="label" data-i18n="types">Typen</div>
    <div id="mats"></div>
    <button id="addmat" class="sec" data-i18n="add_type">+ Typ</button>
  </div>
  <div class="card">
    <div class="label" data-i18n="makers">Hersteller</div>
    <div id="makers"></div>
    <button id="addmaker" class="sec" data-i18n="add_maker">+ Hersteller</button>
  </div>
  <div class="card">
    <div class="label" data-i18n="colors">Farben</div>
    <div id="colors"></div>
    <button id="addcolor" class="sec" data-i18n="add_color">+ Farbe</button>
  </div>
  <div class="row"><button id="savecat" data-i18n="save_catalog">Katalog speichern</button><button id="loadcat" class="sec" data-i18n="discard">Änderungen verwerfen</button></div>
</section>

<!-- Spoolman -->
<section class="page" id="p-spoolman">
  <div class="card">
    <div class="label" data-i18n="server">Server</div>
    <div class="row"><input id="smurl" placeholder="http://spoolman.local:7912" style="width:100%;max-width:24em">
      <button id="smsave" data-i18n="save">Speichern</button><button id="smcheck" class="sec" data-i18n="check">Prüfen</button></div>
    <div id="smstate" style="margin-top:10px"></div>
  </div>
  <div class="card"><div class="label" data-i18n="last_scan">Zuletzt gescannte Spule</div><div id="lastscan2">-</div></div>
  <div class="card">
    <div class="label" data-i18n="queue">Offline-Warteschlange</div>
    <p class="hint" data-i18n="queue_hint"></p>
    <div id="queueinfo" class="val">-</div>
    <div class="row" style="margin-top:12px"><button id="qflush" class="sec" data-i18n="send_now">Jetzt senden</button><button id="qclear" class="sec" data-i18n="discard_all">Alle verwerfen</button></div>
  </div>
</section>

<!-- WLAN -->
<section class="page" id="p-wifi">
  <div class="card">
    <div class="label" data-i18n="connection">Verbindung</div>
    <table>
      <tr><td data-i18n="connected_to">Verbunden mit</td><td id="w_ssid">-</td></tr>
      <tr><td data-i18n="credentials_from">Zugangsdaten aus</td><td id="w_source">-</td></tr>
      <tr><td data-i18n="setup_wifi">Hotspot</td><td id="w_ap">-</td></tr>
    </table>
    <p class="hint" style="margin-top:12px" data-i18n="ap_hint"></p>
    <div class="row"><button class="sec" id="apon" data-i18n="ap_start">Hotspot starten</button><button class="sec" id="apoff" data-i18n="ap_stop">Hotspot beenden</button></div>
  </div>
  <div class="card">
    <div class="label" data-i18n="change_wifi">WLAN ändern</div>
    <p class="hint" data-i18n="wifi_hint"></p>
    <div class="row" style="margin-bottom:10px"><button class="sec" id="scan" data-i18n="scan">Netzwerke suchen</button><span id="scanmsg" style="color:var(--dim)"></span></div>
    <div id="nets"></div>
    <div class="row" style="margin-top:10px"><input id="wssid" data-i18n-ph="ssid" maxlength="32" style="width:14em"><input id="wpass" type="password" data-i18n-ph="password" maxlength="63" style="width:14em"></div>
    <div class="row" style="margin-top:12px"><button id="wsave" data-i18n="save_restart">Speichern &amp; neu starten</button><button id="wreset" class="sec" data-i18n="wifi_reset">WLAN-Daten löschen</button></div>
  </div>
</section>

<!-- Log -->
<section class="page" id="p-log">
  <div class="card">
    <div class="row" style="margin-bottom:10px"><button class="sec" id="logpause" data-i18n="pause">Pause</button><button class="sec" id="logclear" data-i18n="clear_view">Anzeige leeren</button><button class="sec" id="logdl" data-i18n="download">Herunterladen</button></div>
    <pre id="logbox"></pre>
  </div>
</section>

<!-- System -->
<section class="page" id="p-system">
  <div class="card">
    <div class="label" data-i18n="dg_title">Selbstdiagnose</div>
    <p class="hint" style="margin:0 0 10px" data-i18n="dg_hint"></p>
    <div id="diag"></div>
  </div>
  <div class="card">
    <div class="label" data-i18n="mode">Betriebsart</div>
    <label class="row"><input type="radio" name="devmode" id="msmart"><span data-i18n="mode_smart">Smarte Waage</span></label>
    <label class="row"><input type="radio" name="devmode" id="msimple"><span data-i18n="mode_simple">Einfache Waage</span></label>
    <p class="hint" style="margin:10px 0 0" data-i18n="mode_hint"></p>
  </div>
  <div class="card">
    <div class="label" data-i18n="language">Sprache</div>
    <div class="row"><select id="lang"><option value="de">Deutsch</option><option value="en">English</option></select></div>
    <p class="hint" style="margin:10px 0 0" data-i18n="lang_hint"></p>
  </div>
  <div class="card">
    <div class="label" data-i18n="screensaver">Bildschirmschoner</div>
    <p class="hint" data-i18n="ss_hint"></p>
    <div class="row"><span data-i18n="dim_after">Dimmen nach</span><input id="ssdim" type="number" min="0" max="240"><span>min</span>
      <span data-i18n="off_after">Aus nach</span><input id="ssoff" type="number" min="0" max="240"><span>min</span>
      <button id="sssave" data-i18n="save">Speichern</button></div>
  </div>
  <div class="card">
    <div class="label">Theme (theme.json)</div>
    <p class="hint" data-i18n="theme_hint"></p>
    <textarea id="theme" spellcheck="false"></textarea>
    <div class="row" style="margin-top:10px"><button id="save" data-i18n="save_restart">Speichern &amp; neu starten</button><button id="load" class="sec" data-i18n="reload">Neu laden</button></div>
  </div>
  <div class="card" id="fwcard">
    <div class="label" data-i18n="fw_update">Firmware-Update</div>
    <p class="hint" data-i18n="fw_hint"></p>
    <table>
      <tr><td data-i18n="fw_installed">Installiert</td><td id="fwcur">-</td></tr>
      <tr><td data-i18n="fw_latest">Neueste Version im Kanal</td><td id="fwlatest">-</td></tr>
    </table>
    <div class="row" style="margin-top:12px"><span data-i18n="fw_channel">Update-Kanal</span>
      <select id="fwchan"><option value="stable" data-i18n="ch_stable">Stable</option><option value="rc" data-i18n="ch_rc">Release-Kandidat</option><option value="test" data-i18n="ch_test">Test</option></select></div>
    <p class="hint" style="margin:8px 0 0" data-i18n="fw_chan_hint"></p>
    <div id="fwmsg" style="min-height:1.4em;margin-top:10px"></div>
    <progress id="fwbar" max="100" value="0" style="display:none"></progress>
    <div class="row" style="margin-top:12px">
      <button id="fwcheck" class="sec" data-i18n="fw_check">Nach Updates suchen</button>
      <button id="fwinstall" style="display:none" data-i18n="fw_install">Jetzt aktualisieren</button>
      <button id="fwdown" class="sec" style="display:none"></button>
      <a id="fwpage" href="#" target="_blank" rel="noopener" style="display:none" data-i18n="fw_page">Release auf GitHub</a>
    </div>
    <pre id="fwnotes" style="display:none"></pre>
  </div>
  <div class="card">
    <div class="label" data-i18n="maintenance">Wartung</div>
    <table><tr><td data-i18n="fw_slot">Firmware-Slot</td><td id="slot">-</td></tr></table>
    <div class="row" style="margin-top:14px">
      <button id="reboot" class="sec" data-i18n="reboot">Gerät neu starten</button>
      <button id="touchreset" class="sec" data-i18n="touch_reset">Touch-Kalibrierung zurücksetzen</button>
    </div>
  </div>
</section>
</main>
</div>
<div id="toast"></div>

<script>
const $=id=>document.getElementById(id);
const sleep=ms=>new Promise(r=>setTimeout(r,ms));

/* ---- Übersetzung ---- */
const I18N={
de:{
m_overview:'Übersicht',m_calibration:'Kalibrierung',m_users:'Benutzer',m_catalog:'Filament-Katalog',m_spoolman:'Spoolman',m_wifi:'WLAN',m_log:'Log',m_system:'System',
weight:'Gewicht',tare:'Tara',cur_user:'Angemeldeter Benutzer',nobody:'niemand',last_scan:'Zuletzt gescannte Spule',device:'Gerät',wlan:'WLAN',ip:'IP-Adresse',signal:'Signal',uptime:'Laufzeit',mem:'Freier RAM / PSRAM',
status:'Status',raw_net:'Rohwert (netto)',procedure:'Ablauf',cal1:'Waage leeren und <b>Tara</b> drücken.',cal2:'Ein Gewicht mit bekannter Masse auflegen (je schwerer, desto genauer).',cal3:'Die Masse eingeben und <b>Kalibrieren</b> drücken.',calibrate:'Kalibrieren',
no_sensor:'{0} nicht gefunden',sensor:'Wägesensor',sensor_save:'Speichern und neu starten',sensor_restart:'Gespeicherte Auswahl ist noch nicht aktiv, Neustart nötig.',sensor_cal_hint:'Die Kalibrierung wird pro Sensortyp gespeichert. Nach dem ersten Wechsel auf einen Sensor neu kalibrieren.',sensor_confirm:'Sensor {0} speichern und Gerät neu starten?',sensor_none:'– (nicht belegt)',sensor_bus:'Touch-Bus',sh_hx711:'Standard. Versorgung 3,3 V (DOUT darf keine 5 V führen). HX710B-Module laufen mit demselben Treiber.',sh_nau7802:'I²C, Adresse 0x2A, 3,3 V. SCL = GPIO47 und SDA = GPIO48 nutzt den Touch-Bus mit (Header P2 Pin 5 und 6, keine weiteren Pins), andere Pins bekommen einen eigenen Bus. Am Gerät nicht geprüft.',sh_ads1232:'2-Draht wie HX711. PDWN ist optional (sonst am Modul fest auf 3,3 V). Verstärkung und Datenrate (10 SPS) per Lötbrücke am Modul. Am Gerät nicht geprüft.',not_calibrated:'Nicht kalibriert',calibrated:'Kalibriert',overload:'Überlast',stable:'stabil',no_connection:'Keine Verbindung',
users_hint:'Ein Benutzer-Tag an der Waage meldet den Benutzer an, gescanntes Filament gehört dann ihm. Abmeldung nach 15 Minuten ohne Scan oder durch Tippen auf den Namen am Gerät.',
name:'Name',create:'Anlegen',write_tag:'Tag schreiben',delete:'Löschen',no_users:'Noch keine Benutzer.',confirm_del_user:'Benutzer "{0}" löschen? Sein Tag wird damit ungültig.',user_created:'Benutzer angelegt',
admin:'Admin',admin_hint:'Nur der Admin legt Benutzer an und ändert Einstellungen, am Gerät mit der Geräte-PIN (Ziffernblock) oder dem Admin-Tag, hier mit Benutzer "admin" und dem Passwort. Ist die Sperre aus, darf jeder alles ändern. Notfall: serieller Befehl "admin reset" (USB).',admin_default_warn:'Das Standardpasswort ist noch aktiv. Bitte ein eigenes Passwort setzen.',admin_lock:'Admin-Sperre aktiv',new_password:'Neues Passwort',change_password:'Passwort ändern',write_admin_tag:'Admin-Tag schreiben',confirm_unlock:'Sperre abschalten? Danach darf jeder im Netzwerk und am Gerät alle Einstellungen ändern.',lock_on:'Sperre eingeschaltet',lock_off:'Sperre abgeschaltet',pw_changed:'Passwort geändert (der Browser fragt beim nächsten Speichern neu)',lang_device:'Gerätesprache',dev_pin:'Geräte-PIN (4 bis 6 Ziffern)',set_dev_pin:'Geräte-PIN setzen',dev_pin_warn:'Die Standard-Geräte-PIN (1234) ist noch aktiv. Bitte eine eigene PIN setzen.',dev_pin_saved:'Geräte-PIN gespeichert',sync_title:'Abgleich mit Spoolman',sync_hint:'Neue Hersteller und Filamenttypen aus Spoolman werden automatisch alle 10 Minuten übernommen. Vorhandene Einträge bleiben unverändert, es wird nichts gelöscht. Der Temperaturbereich kommt aus der Düsentemperatur der Filamente in Spoolman (bei nur einem Wert +/- 10 °C), sonst aus üblichen Werten. Bitte bei Bedarf prüfen.',sync_now:'Jetzt abgleichen',sync_running:'Abgleich läuft ...',sync_ok:'Abgleich: {0} neue Hersteller, {1} neue Typen (gelesen: {2} Hersteller, {3} Typen)',sync_skip:', {0} nicht übernommen (Name zu lang oder Liste voll)',sync_err:'Abgleich fehlgeschlagen: {0}',sync_never:'Noch kein Abgleich seit dem Start',sync_ago:' · vor {0} s',cat_right:'Darf den Filament-Katalog pflegen',filament:'Filament',at_device:'Am Gerät',login_title:'Anmelden',login_name:'Name',login_secret:'PIN / Passwort',login:'Anmelden',logout:'Abmelden',generic_login:'Generic (nur ansehen)',wifi_connected:'Verbunden',wifi_disconnected:'Nicht verbunden',my_settings:'Meine Einstellungen',my_lang:'Meine Sprache',old_pin:'Alte PIN',new_pin:'Neue PIN',change_pin:'PIN ändern',pin_changed:'PIN geändert',pin_rules:'PIN: 4 bis 6 Ziffern.',pin_set:'PIN setzen',pin_change:'PIN ändern',pin_prompt:'Neue PIN (4 bis 6 Ziffern, leer = PIN entfernen):',pin_optional:'PIN (optional)',generic:'Generic-Zugang',generic_on:'Generic-Zugang aktiv (nur wiegen und NFC lesen)',set_pin:'PIN setzen',pin_saved:'PIN gespeichert',no_pin_hint:'ohne PIN kein Zugang über die Webseite',tare_right:'Darf am Gerät tarieren',k_admin:'Admin',
cat_hint:'Auswahl am Gerät unter NFC &rarr; Filament. Typen mit Temperaturbereich in &deg;C (min / max).',types:'Typen',add_type:'+ Typ',makers:'Hersteller',add_maker:'+ Hersteller',colors:'Farben',add_color:'+ Farbe',save_catalog:'Katalog speichern',discard:'Änderungen verwerfen',
ph_type:'Typ',ph_min:'min',ph_max:'max',ph_maker:'Hersteller',ph_color:'Farbname',remove:'Entfernen',catalog_saved:'Katalog gespeichert',
server:'Server',save:'Speichern',check:'Prüfen',checking:'Prüfe ...',reachable:'Erreichbar (Version {0})',unreachable:'Nicht erreichbar',version:'Version {0}',unreach_short:'nicht erreichbar',pending:'{0} ausstehend',
queue:'Offline-Warteschlange',queue_hint:'Updates, die ohne WLAN oder Spoolman nicht gesendet werden können, werden gespeichert und automatisch nachgeholt. Je Spule bleibt nur das neueste Update.',queue_info:'{0} ausstehend',queue_dropped:', {0} verworfen (dauerhafter Fehler)',send_now:'Jetzt senden',discard_all:'Alle verwerfen',confirm_clear_queue:'Alle ausstehenden Updates verwerfen?',
scan_unknown:'Tag ist in Spoolman nicht bekannt ({0})',scan_error:'Spoolman-Fehler: {0}',owner:'Besitzer',url_saved:'Spoolman-URL gespeichert',
connection:'Verbindung',connected_to:'Verbunden mit',credentials_from:'Zugangsdaten aus',src_nvs:'Gespeichert',src_none:'Keine gespeichert (Hotspot)',setup_wifi:'Hotspot',ap_active:'Hotspot aktiv: {0} ({1})',ap_inactive:'aus',
ap_hint:'Sind keine WLAN-Daten gespeichert oder kann sich das Gerät eine Minute lang nicht verbinden, öffnet es einen eigenen Hotspot. Das Passwort zeigt die Waage auf dem Display an (auch unter Einstellungen → WiFi). Nach dem Verbinden öffnet sich die Einrichtungsseite von selbst, sonst unter http://192.168.4.1 aufrufen.',ap_start:'Hotspot starten',ap_stop:'Hotspot beenden',
change_wifi:'WLAN ändern',wifi_hint:'Das Gerät speichert die Zugangsdaten und startet neu. Falsche Daten sind unproblematisch: Nach einer Minute ohne Verbindung öffnet das Gerät wieder den Hotspot.',scan:'Netzwerke suchen',scanning:'Suche ...',ssid:'Netzwerkname (SSID)',password:'Passwort',save_restart:'Speichern & neu starten',wifi_reset:'WLAN-Daten löschen',
wifi_saved:'Gespeichert, Gerät startet neu und verbindet sich ...',confirm_wifi_reset:'Gespeicherte WLAN-Daten löschen? Das Gerät startet neu und öffnet den Hotspot, bis ein WLAN eingerichtet ist.',no_networks:'Keine Netzwerke gefunden',ssid_missing:'Bitte einen Netzwerknamen eingeben',
pause:'Pause',resume:'Weiter',clear_view:'Anzeige leeren',download:'Herunterladen',
mode:'Betriebsart',mode_smart:'Smarte Waage (NFC, Anmeldung, Spoolman)',mode_simple:'Einfache Waage (nur wiegen, ohne Anmeldung und Spoolman)',mode_hint:'Einfache Waage: Am Gerät gibt es nur "Filament wiegen" (Spule allein oder Box + Spule). Benutzer-Tags melden nicht an, Spulen werden nicht in Spoolman nachgeschlagen, Spoolman wird nicht abgefragt. Jeder darf tarieren. Einstellungen bleiben beim Admin.',mode_set:'Betriebsart gespeichert',sm_off:'aus (einfache Waage)',weigh_only:'Nur wiegen',weigh_only_right:'Sieht am Gerät nur "Filament wiegen", ohne Spoolman',
language:'Sprache',lang_hint:'Gilt für die Anzeige am Gerät (es springt dabei zum Startbildschirm) und für diese Webseite.',lang_set:'Sprache auf Deutsch gestellt',
screensaver:'Bildschirmschoner',ss_hint:'Nach der eingestellten Zeit ohne Berührung, Tag oder Gewichtsänderung wird die Anzeige gedimmt, später ausgeschaltet. 0 = nie. Der erste Tipp weckt nur auf.',dim_after:'Dimmen nach',off_after:'Aus nach',ss_saved:'Bildschirmschoner gespeichert',
theme_hint:'Farben, Radius, Abstände und Schriftgrößen der Anzeige am Gerät. Nach dem Speichern startet das Gerät neu.',reload:'Neu laden',maintenance:'Wartung',fw_slot:'Firmware-Slot',fw_update:'Firmware-Update',fw_hint:'Sucht auf GitHub nach der neuesten Version im gewählten Kanal und installiert sie per WLAN. Einstellungen, Benutzer und Kalibrierung bleiben erhalten. Schlägt das Update fehl, läuft die bisherige Firmware weiter.',fw_channel:'Update-Kanal',ch_stable:'Stable',ch_rc:'Release-Kandidat',ch_test:'Test',fw_chan_hint:'Stable: nur normale Releases (v1.2). Release-Kandidat: zusätzlich Tags mit -rc (v1.3-rc1). Test: zusätzlich alle anderen Vorabversionen (v1.3-beta1, v1.3-test2).',fw_down:'{0} installieren (Rückschritt)',fw_down_info:'Im Kanal gibt es nur die ältere Version {0}.',fw_down_confirm:'Version {0} ist älter als die installierte. Trotzdem installieren? Das Gerät startet danach neu.',fw_chan_set:'Kanal gespeichert',fw_installed:'Installiert',fw_latest:'Neueste Version im Kanal',fw_check:'Nach Updates suchen',fw_install:'Jetzt aktualisieren',fw_page:'Release auf GitHub',fw_checking:'Suche nach Updates ...',fw_uptodate:'Die Firmware ist aktuell.',fw_avail:'Version {0} ist verfügbar.',fw_unknown:'Installierte Version unbekannt (Entwicklungsstand). Version {0} kann installiert werden.',fw_norel:'Auf GitHub gibt es in diesem Kanal noch keine Version.',fw_never:'Noch nicht gesucht.',fw_dl:'Update wird geladen und geschrieben: {0} %. Gerät nicht ausschalten.',fw_done:'Update installiert, Gerät startet neu ...',fw_confirm:'Version {0} jetzt installieren? Das Gerät startet danach neu.',fw_back:'Neustart fertig, Version {0} läuft.',fw_ago:'zuletzt vor {0}',fw_cut:'(gekürzt)',reboot:'Gerät neu starten',touch_reset:'Touch-Kalibrierung zurücksetzen',
saved_restarting:'Gespeichert, Gerät startet neu ...',confirm_reboot:'Gerät neu starten?',rebooting:'Neustart ...',confirm_touch:'Touch-Kalibrierung zurücksetzen? Danach am Gerät neu kalibrieren (Menü → Einstellungen → Touch kalibrieren).',touch_done:'Touch-Kalibrierung zurückgesetzt',
tare_done:'Tara gesetzt',tare_fail:'Tara fehlgeschlagen: {0}',cal_done:'Kalibriert',cal_fail:'Kalibrierung fehlgeschlagen: {0}',
nfc_none:'Modul nicht gefunden',nfc_ready:'bereit',k_spool:'Spule',k_box:'Box',k_user:'Benutzer',k_unknown:'Tag ohne Format',
w_waiting:'Tag an die Waage halten (vorher entfernen und neu auflegen) ...',w_writing:'Schreibe ...',w_ok:'Tag geschrieben',w_locked:'Tag ist gesperrt',w_too_big:'Passt nicht auf den Tag',w_not_ntag:'Kein NTAG erkannt',w_failed:'Schreiben fehlgeschlagen',w_timeout:'Kein Tag erkannt, Zeit abgelaufen',w_occupied:'Der Tag enthält eine Spule und wurde nicht überschrieben',w_nomodule:'NFC-Modul nicht gefunden',
err:'Fehler: {0}',load_fail:'Laden fehlgeschlagen: {0}',save_fail:'Speichern fehlgeschlagen: {0}',invalid_json:'Kein gültiges JSON: {0}',checking_sm:'Prüfe Spoolman ...',
dg_title:'Selbstdiagnose',dg_hint:'Prüft Wägesensor, NFC-Modul, WLAN und Spoolman und nennt bei einem Fehler die wahrscheinliche Ursache. Die Seite aktualisiert sich alle 3 Sekunden.',dg_c_sensor:'Wägesensor',dg_c_nfc:'NFC-Modul (PN532)',dg_c_wifi:'WLAN',dg_c_spoolman:'Spoolman',dg_c_queue:'Offline-Warteschlange',dg_sensor_ok:'Der Wägesensor liefert Messwerte.',dg_sensor_starting:'Der Wägesensor startet noch.',dg_sensor_no_data:'Der Wägesensor liefert keine gültigen Werte.',dg_sensor_no_data_tip:'Verkabelung und Versorgung prüfen (Stecker, Lötstellen, 3,3 V bzw. 5 V, GND) und die Pins unter Kalibrierung. Beim HX711 helfen Kondensatoren 100 µF und 100 nF an der Versorgung. Details im Log ("ungültige Lesungen").',dg_sensor_not_calibrated:'Der Wägesensor ist nicht kalibriert.',dg_sensor_not_calibrated_tip:'Unter Kalibrierung zuerst tarieren, dann ein bekanntes Gewicht auflegen und kalibrieren.',dg_sensor_overload:'Der Wägesensor ist überlastet.',dg_sensor_overload_tip:'Gewicht von der Waage nehmen. Bleibt die Meldung, Kalibrierung und Nullpunkt prüfen, ggf. neu tarieren.',dg_nfc_ok:'Das NFC-Modul antwortet.',dg_nfc_starting:'Das NFC-Modul startet noch.',dg_nfc_no_module:'Das NFC-Modul (PN532) antwortet nicht.',dg_nfc_no_module_tip:'DIP-Schalter am Modul auf SPI stellen (SW1 aus, SW2 an), Verkabelung SCK/MISO/MOSI/SS und 3,3 V prüfen, Modul stromlos machen und neu starten. Das Gerät versucht es alle paar Sekunden erneut.',dg_wifi_ok:'Das WLAN ist verbunden.',dg_wifi_weak:'Das WLAN-Signal ist schwach.',dg_wifi_weak_tip:'Waage näher an den Router stellen oder einen Repeater nutzen. Bei schwachem Signal brechen Anfragen an Spoolman öfter ab.',dg_wifi_starting:'Das Gerät verbindet sich noch mit dem WLAN.',dg_wifi_no_creds:'Es sind keine WLAN-Zugangsdaten gespeichert.',dg_wifi_no_creds_tip:'Mit dem Hotspot der Waage verbinden und unter WLAN Netz und Passwort eintragen.',dg_wifi_disconnected:'Das Gerät ist nicht mit dem WLAN verbunden.',dg_wifi_disconnected_tip:'Router eingeschaltet und in Reichweite? Name und Passwort unter WLAN prüfen (nur 2,4 GHz). Nach einer Minute startet der Einrichtungs-Hotspot.',dg_spoolman_ok:'Spoolman ist erreichbar.',dg_spoolman_starting:'Die erste Prüfung von Spoolman läuft noch.',dg_spoolman_no_wifi:'Spoolman kann ohne WLAN nicht erreicht werden.',dg_spoolman_no_wifi_tip:'Zuerst das WLAN-Problem beheben.',dg_spoolman_unreachable:'Spoolman antwortet nicht.',dg_spoolman_unreachable_tip:'Läuft Spoolman (Container/Server)? Adresse und Port unter Spoolman prüfen (z. B. http://spoolman.local:7912), Waage und Server im selben Netz, Firewall offen. Nur http:// wird unterstützt.',dg_spoolman_wrong_url:'Unter dieser Adresse gibt es kein Spoolman (HTTP 404).',dg_spoolman_wrong_url_tip:'Die Adresse unter Spoolman prüfen, ohne Pfad wie /api oder /spoolman.',dg_spoolman_denied:'Spoolman verweigert den Zugriff (HTTP {0}).',dg_spoolman_denied_tip:'Ein Proxy oder eine Anmeldung steht davor. Die Waage kann sich nicht anmelden: direkte Adresse und Port von Spoolman verwenden.',dg_spoolman_server_error:'Spoolman meldet einen Serverfehler (HTTP {0}).',dg_spoolman_server_error_tip:'Log und Datenbank von Spoolman prüfen, ggf. Spoolman neu starten. Bei einem Proxy davor dessen Ziel prüfen.',dg_spoolman_not_spoolman:'Die Adresse antwortet, ist aber kein Spoolman.',dg_spoolman_not_spoolman_tip:'Unter dieser Adresse läuft ein anderer Dienst. Port prüfen (Spoolman: 7912) und Version von Spoolman (ab 0.20).',dg_spoolman_http_error:'Spoolman antwortet unerwartet (HTTP {0}).',dg_spoolman_http_error_tip:'Adresse und Spoolman-Version prüfen. Die Waage ist gegen Spoolman 0.27 getestet.',dg_queue_ok:'Es warten keine Updates.',dg_queue_sending:'Ausstehende Updates werden gesendet.',dg_queue_waiting:'Updates warten, weil Spoolman nicht erreichbar ist.',dg_queue_waiting_tip:'Sie bleiben gespeichert und werden gesendet, sobald Spoolman wieder erreichbar ist.',dg_queue_dropped:'Updates wurden verworfen (dauerhafter Fehler).',dg_queue_dropped_tip:'Spoolman hat sie abgelehnt, z. B. weil die Spule gelöscht wurde. Details im Log.',dg_sensor_no_data_tip_nau7802:'NAU7802 (I²C): Verkabelung SCL/SDA und Versorgung prüfen, I²C-Adresse 0x2A, Pull-ups vorhanden? Sensortyp und Pins unter Kalibrierung müssen zum Aufbau passen. Das Gerät sucht den Chip alle 2 s erneut. Details im Log.',dg_sensor_no_data_tip_ads1232:'ADS1232/ADS1231: SCLK, DOUT und Versorgung prüfen. Ist PDWN nicht fest auf 3,3 V gelegt, muss der Pin unter Kalibrierung gesetzt sein, sonst bleibt der Wandler im Ruhemodus. Sensortyp und Pins müssen zum Aufbau passen.'
},
en:{
m_overview:'Overview',m_calibration:'Calibration',m_users:'Users',m_catalog:'Filament catalog',m_spoolman:'Spoolman',m_wifi:'WiFi',m_log:'Log',m_system:'System',
weight:'Weight',tare:'Tare',cur_user:'Logged-in user',nobody:'nobody',last_scan:'Last scanned spool',device:'Device',wlan:'WiFi',ip:'IP address',signal:'Signal',uptime:'Uptime',mem:'Free RAM / PSRAM',
status:'Status',raw_net:'Raw value (net)',procedure:'Procedure',cal1:'Empty the scale and press <b>Tare</b>.',cal2:'Place a weight of known mass (the heavier, the more accurate).',cal3:'Enter the mass and press <b>Calibrate</b>.',calibrate:'Calibrate',
no_sensor:'{0} not found',sensor:'Load sensor',sensor_save:'Save and restart',sensor_restart:'The saved selection is not active yet, restart required.',sensor_cal_hint:'Calibration is stored per sensor type. Calibrate again after switching to a sensor for the first time.',sensor_confirm:'Save sensor {0} and restart the device?',sensor_none:'– (not connected)',sensor_bus:'touch bus',sh_hx711:'Default. Supply 3.3 V (DOUT must not carry 5 V). HX710B modules work with the same driver.',sh_nau7802:'I²C, address 0x2A, 3.3 V. SCL = GPIO47 and SDA = GPIO48 shares the touch bus (header P2 pins 5 and 6, no extra pins), other pins get a bus of their own. Not tested on the device.',sh_ads1232:'2-wire like the HX711. PDWN is optional (otherwise tied to 3.3 V on the module). Gain and data rate (10 SPS) are set by solder jumpers on the module. Not tested on the device.',not_calibrated:'Not calibrated',calibrated:'Calibrated',overload:'Overload',stable:'stable',no_connection:'No connection',
users_hint:'A user tag held to the scale logs the user in; scanned filament then belongs to them. Logout after 15 minutes without a scan or by tapping the name on the device.',
name:'Name',create:'Create',write_tag:'Write tag',delete:'Delete',no_users:'No users yet.',confirm_del_user:'Delete user "{0}"? Their tag becomes invalid.',user_created:'User created',
admin:'Admin',admin_hint:'Only the admin creates users and changes settings, on the device with the device PIN (number pad) or admin tag, here with user "admin" and the password. With the lock off, everyone may change everything. Emergency: serial command "admin reset" (USB).',admin_default_warn:'The default password is still active. Please set your own password.',admin_lock:'Admin lock active',new_password:'New password',change_password:'Change password',write_admin_tag:'Write admin tag',confirm_unlock:'Turn the lock off? Everyone on the network and at the device may then change all settings.',lock_on:'Lock turned on',lock_off:'Lock turned off',pw_changed:'Password changed (the browser will ask again on the next save)',lang_device:'Device language',dev_pin:'Device PIN (4 to 6 digits)',set_dev_pin:'Set device PIN',dev_pin_warn:'The default device PIN (1234) is still active. Please set your own PIN.',dev_pin_saved:'Device PIN saved',sync_title:'Sync with Spoolman',sync_hint:'New manufacturers and filament types from Spoolman are imported automatically every 10 minutes. Existing entries stay unchanged and nothing is deleted. The temperature range comes from the nozzle temperature of the filaments in Spoolman (+/- 10 °C if there is only one value), otherwise from common values. Please check if needed.',sync_now:'Sync now',sync_running:'Syncing ...',sync_ok:'Sync: {0} new manufacturers, {1} new types (read: {2} manufacturers, {3} types)',sync_skip:', {0} not imported (name too long or list full)',sync_err:'Sync failed: {0}',sync_never:'No sync since start yet',sync_ago:' · {0} s ago',cat_right:'May maintain the filament catalog',filament:'Filament',at_device:'At the device',login_title:'Log in',login_name:'Name',login_secret:'PIN / password',login:'Log in',logout:'Log out',generic_login:'Generic (view only)',wifi_connected:'Connected',wifi_disconnected:'Not connected',my_settings:'My settings',my_lang:'My language',old_pin:'Old PIN',new_pin:'New PIN',change_pin:'Change PIN',pin_changed:'PIN changed',pin_rules:'PIN: 4 to 6 digits.',pin_set:'Set PIN',pin_change:'Change PIN',pin_prompt:'New PIN (4 to 6 digits, empty = remove PIN):',pin_optional:'PIN (optional)',generic:'Generic access',generic_on:'Generic access active (weigh and read NFC only)',set_pin:'Set PIN',pin_saved:'PIN saved',no_pin_hint:'without a PIN there is no web access',tare_right:'May tare at the device',k_admin:'Admin',
cat_hint:'Selected on the device under NFC &rarr; Filament. Types with temperature range in &deg;C (min / max).',types:'Types',add_type:'+ Type',makers:'Manufacturers',add_maker:'+ Manufacturer',colors:'Colors',add_color:'+ Color',save_catalog:'Save catalog',discard:'Discard changes',
ph_type:'Type',ph_min:'min',ph_max:'max',ph_maker:'Manufacturer',ph_color:'Color name',remove:'Remove',catalog_saved:'Catalog saved',
server:'Server',save:'Save',check:'Check',checking:'Checking ...',reachable:'Reachable (version {0})',unreachable:'Not reachable',version:'Version {0}',unreach_short:'not reachable',pending:'{0} pending',
queue:'Offline queue',queue_hint:'Updates that cannot be sent without WiFi or Spoolman are stored and sent automatically later. Only the latest update per spool is kept.',queue_info:'{0} pending',queue_dropped:', {0} discarded (permanent error)',send_now:'Send now',discard_all:'Discard all',confirm_clear_queue:'Discard all pending updates?',
scan_unknown:'Tag is not known to Spoolman ({0})',scan_error:'Spoolman error: {0}',owner:'owner',url_saved:'Spoolman URL saved',
connection:'Connection',connected_to:'Connected to',credentials_from:'Credentials from',src_nvs:'Saved',src_none:'None saved (hotspot)',setup_wifi:'Hotspot',ap_active:'Hotspot active: {0} ({1})',ap_inactive:'off',
ap_hint:'If no WiFi credentials are saved, or the device cannot connect for a minute, it opens its own hotspot. The password is shown on the scale\'s display (also under Settings → WiFi). After joining, the setup page opens by itself, otherwise browse to http://192.168.4.1.',ap_start:'Start hotspot',ap_stop:'Stop hotspot',
change_wifi:'Change WiFi',wifi_hint:'The device stores the credentials and restarts. Wrong credentials are not a problem: after a minute without a connection the device opens the hotspot again.',scan:'Scan networks',scanning:'Scanning ...',ssid:'Network name (SSID)',password:'Password',save_restart:'Save & restart',wifi_reset:'Delete WiFi credentials',
wifi_saved:'Saved, device restarts and connects ...',confirm_wifi_reset:'Delete saved WiFi credentials? The device restarts and opens the hotspot until a WiFi is set up.',no_networks:'No networks found',ssid_missing:'Please enter a network name',
pause:'Pause',resume:'Resume',clear_view:'Clear view',download:'Download',
mode:'Operating mode',mode_smart:'Smart scale (NFC, login, Spoolman)',mode_simple:'Simple scale (weigh only, no login, no Spoolman)',mode_hint:'Simple scale: the device only offers "Weigh filament" (spool alone or box + spool). User tags do not log in, spools are not looked up in Spoolman and Spoolman is not contacted. Everyone may tare. Settings stay with the admin.',mode_set:'Operating mode saved',sm_off:'off (simple scale)',weigh_only:'Weigh only',weigh_only_right:'Only sees "Weigh filament" on the device, no Spoolman',
language:'Language',lang_hint:'Applies to the display on the device (it jumps to the home screen) and to this web page.',lang_set:'Language set to English',
screensaver:'Screensaver',ss_hint:'After the set time without touch, tag or weight change the display is dimmed, later switched off. 0 = never. The first tap only wakes it up.',dim_after:'Dim after',off_after:'Off after',ss_saved:'Screensaver saved',
theme_hint:'Colors, radius, spacing and font sizes of the display on the device. The device restarts after saving.',reload:'Reload',maintenance:'Maintenance',fw_slot:'Firmware slot',fw_update:'Firmware update',fw_hint:'Looks up the latest version of the selected channel on GitHub and installs it over Wi-Fi. Settings, users and calibration are kept. If the update fails, the current firmware keeps running.',fw_channel:'Update channel',ch_stable:'Stable',ch_rc:'Release candidate',ch_test:'Test',fw_chan_hint:'Stable: regular releases only (v1.2). Release candidate: also tags with -rc (v1.3-rc1). Test: also all other pre-releases (v1.3-beta1, v1.3-test2).',fw_down:'Install {0} (downgrade)',fw_down_info:'This channel only has the older version {0}.',fw_down_confirm:'Version {0} is older than the installed one. Install anyway? The device restarts afterwards.',fw_chan_set:'Channel saved',fw_installed:'Installed',fw_latest:'Latest version in channel',fw_check:'Check for updates',fw_install:'Update now',fw_page:'Release on GitHub',fw_checking:'Checking for updates ...',fw_uptodate:'The firmware is up to date.',fw_avail:'Version {0} is available.',fw_unknown:'Installed version unknown (development build). Version {0} can be installed.',fw_norel:'There is no version in this channel on GitHub yet.',fw_never:'Not checked yet.',fw_dl:'Downloading and writing update: {0} %. Do not switch off the device.',fw_done:'Update installed, device restarts ...',fw_confirm:'Install version {0} now? The device restarts afterwards.',fw_back:'Restart finished, version {0} is running.',fw_ago:'last checked {0} ago',fw_cut:'(shortened)',reboot:'Restart device',touch_reset:'Reset touch calibration',
saved_restarting:'Saved, device restarts ...',confirm_reboot:'Restart the device?',rebooting:'Restarting ...',confirm_touch:'Reset touch calibration? Then recalibrate on the device (Menu → Settings → Calibrate touch).',touch_done:'Touch calibration reset',
tare_done:'Tare set',tare_fail:'Tare failed: {0}',cal_done:'Calibrated',cal_fail:'Calibration failed: {0}',
nfc_none:'Module not found',nfc_ready:'ready',k_spool:'Spool',k_box:'Box',k_user:'User',k_unknown:'Tag without format',
w_waiting:'Hold the tag to the scale (remove and place it again first) ...',w_writing:'Writing ...',w_ok:'Tag written',w_locked:'Tag is locked',w_too_big:'Does not fit on the tag',w_not_ntag:'Not an NTAG',w_failed:'Write failed',w_timeout:'No tag detected, timed out',w_occupied:'The tag holds a spool and was not overwritten',w_nomodule:'NFC module not found',
err:'Error: {0}',load_fail:'Loading failed: {0}',save_fail:'Saving failed: {0}',invalid_json:'Not valid JSON: {0}',checking_sm:'Checking Spoolman ...',
dg_title:'Self-diagnosis',dg_hint:'Checks load sensor, NFC module, Wi-Fi and Spoolman and names the likely cause of a fault. The page refreshes every 3 seconds.',dg_c_sensor:'Load sensor',dg_c_nfc:'NFC module (PN532)',dg_c_wifi:'Wi-Fi',dg_c_spoolman:'Spoolman',dg_c_queue:'Offline queue',dg_sensor_ok:'The load sensor delivers readings.',dg_sensor_starting:'The load sensor is still starting.',dg_sensor_no_data:'The load sensor delivers no valid readings.',dg_sensor_no_data_tip:'Check wiring and supply (connectors, solder joints, 3.3 V or 5 V, GND) and the pins under Calibration. For the HX711, capacitors of 100 µF and 100 nF at the supply help. See the log ("invalid readings").',dg_sensor_not_calibrated:'The load sensor is not calibrated.',dg_sensor_not_calibrated_tip:'Under Calibration, tare first, then put a known weight on and calibrate.',dg_sensor_overload:'The load sensor is overloaded.',dg_sensor_overload_tip:'Take the weight off. If it stays, check calibration and zero point, and tare again if needed.',dg_nfc_ok:'The NFC module responds.',dg_nfc_starting:'The NFC module is still starting.',dg_nfc_no_module:'The NFC module (PN532) does not respond.',dg_nfc_no_module_tip:'Set the DIP switches on the module to SPI (SW1 off, SW2 on), check wiring SCK/MISO/MOSI/SS and 3.3 V, power-cycle the module and restart. The device retries every few seconds.',dg_wifi_ok:'Wi-Fi is connected.',dg_wifi_weak:'The Wi-Fi signal is weak.',dg_wifi_weak_tip:'Move the scale closer to the router or use a repeater. With a weak signal, requests to Spoolman fail more often.',dg_wifi_starting:'The device is still connecting to Wi-Fi.',dg_wifi_no_creds:'No Wi-Fi credentials are saved.',dg_wifi_no_creds_tip:'Connect to the scale\'s hotspot and enter network and password under Wi-Fi.',dg_wifi_disconnected:'The device is not connected to Wi-Fi.',dg_wifi_disconnected_tip:'Is the router on and in range? Check name and password under Wi-Fi (2.4 GHz only). After one minute the setup hotspot starts.',dg_spoolman_ok:'Spoolman is reachable.',dg_spoolman_starting:'The first Spoolman check is still running.',dg_spoolman_no_wifi:'Spoolman cannot be reached without Wi-Fi.',dg_spoolman_no_wifi_tip:'Fix the Wi-Fi problem first.',dg_spoolman_unreachable:'Spoolman does not answer.',dg_spoolman_unreachable_tip:'Is Spoolman (container/server) running? Check address and port under Spoolman (e.g. http://spoolman.local:7912), scale and server on the same network, firewall open. Only http:// is supported.',dg_spoolman_wrong_url:'There is no Spoolman at this address (HTTP 404).',dg_spoolman_wrong_url_tip:'Check the address under Spoolman, without a path such as /api or /spoolman.',dg_spoolman_denied:'Spoolman denies access (HTTP {0}).',dg_spoolman_denied_tip:'A proxy or login is in front of it. The scale cannot log in: use the direct address and port of Spoolman.',dg_spoolman_server_error:'Spoolman reports a server error (HTTP {0}).',dg_spoolman_server_error_tip:'Check the Spoolman log and database, restart Spoolman if needed. With a proxy in front, check its target.',dg_spoolman_not_spoolman:'The address answers but is not Spoolman.',dg_spoolman_not_spoolman_tip:'Another service runs at this address. Check the port (Spoolman: 7912) and the Spoolman version (0.20 or newer).',dg_spoolman_http_error:'Spoolman answers unexpectedly (HTTP {0}).',dg_spoolman_http_error_tip:'Check the address and the Spoolman version. The scale is tested against Spoolman 0.27.',dg_queue_ok:'No updates are waiting.',dg_queue_sending:'Pending updates are being sent.',dg_queue_waiting:'Updates are waiting because Spoolman is not reachable.',dg_queue_waiting_tip:'They stay stored and are sent as soon as Spoolman is reachable again.',dg_queue_dropped:'Updates were discarded (permanent error).',dg_queue_dropped_tip:'Spoolman rejected them, e.g. because the spool was deleted. See the log for details.',dg_sensor_no_data_tip_nau7802:'NAU7802 (I²C): check wiring SCL/SDA and supply, I²C address 0x2A, pull-ups present? Sensor type and pins under Calibration must match the build. The device searches for the chip again every 2 s. See the log.',dg_sensor_no_data_tip_ads1232:'ADS1232/ADS1231: check SCLK, DOUT and supply. If PDWN is not tied to 3.3 V, the pin must be set under Calibration, otherwise the converter stays in power-down. Sensor type and pins must match the build.'
}};
let LANG='de';
const t=(k,...a)=>{let s=I18N[LANG][k];if(s===undefined)s=I18N.de[k];if(s===undefined)s=k;a.forEach((v,i)=>{s=s.split('{'+i+'}').join(v)});return s};
function applyLang(l){
  if(l!=='de'&&l!=='en')return;
  const changed=l!==LANG;LANG=l;document.documentElement.lang=l;
  document.querySelectorAll('[data-i18n]').forEach(e=>{e.textContent=t(e.dataset.i18n)});
  document.querySelectorAll('[data-i18n-html]').forEach(e=>{e.innerHTML=t(e.dataset.i18nHtml)});
  document.querySelectorAll('[data-i18n-ph]').forEach(e=>{e.placeholder=t(e.dataset.i18nPh)});
  show(true);
  if(changed){renderUsers();renderCat();logPauseLabel();if(fwLast)fwRender(fwLast)}
}

let toastTimer;
function say(msg,c){const m=$('toast');m.textContent=msg;m.className='show '+(c||'');clearTimeout(toastTimer);toastTimer=setTimeout(()=>{m.className=''},4500)}
async function api(path,opt){const r=await fetch(path,opt);if(!r.ok){if(r.status===401&&ME)showLogin();throw new Error((await r.text())||('HTTP '+r.status))}return r}
const post=(p,body)=>api(p,{method:'POST',headers:{'Content-Type':'text/plain'},body});

/* ---- Menü ---- */
const PAGES=['overview','calibration','users','catalog','spoolman','wifi','log','system'];
const loaded={};
const ONLOAD={calibration:()=>loadSensor(),users:()=>{loadUsers();loadAdmin()},catalog:()=>{loadCat();loadSync()},system:()=>{loadTheme();loadSS();loadFw(true)},wifi:()=>loadWifi(),log:()=>startLog()};
function show(langOnly){
  let p=location.hash.slice(1);if(!PAGES.includes(p)||(p!=='overview'&&!(ME&&(ME.role==='admin'||(p==='catalog'&&ME.catalog)))))p='overview';
  document.querySelectorAll('.page').forEach(e=>e.classList.toggle('active',e.id==='p-'+p));
  document.querySelectorAll('#menu a').forEach(a=>a.classList.toggle('active',a.dataset.page===p));
  $('title').textContent=t('m_'+p);document.title=t('m_'+p)+' – Filamentwaage';
  if(langOnly===true)return;
  if(!loaded[p]){loaded[p]=true;if(ONLOAD[p])ONLOAD[p]()}
  if(p==='system')startDiag();
  window.scrollTo(0,0);
}
window.addEventListener('hashchange',()=>show());

/* ---- Status ---- */
function dur(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+'d ':'')+h+'h '+m+'m'}
function render(d){
  if(d.lang&&d.lang!==LANG)applyLang(d.lang);
  const st=$('state');let w='-- g',mini='';
  if(!d.valid){st.textContent=t('no_sensor',d.sensor||'Sensor');st.className='err'}
  else if(!d.calibrated){st.textContent=t('not_calibrated');st.className='warn'}
  else{w=(Math.round(d.weight*10)/10).toFixed(1)+' g';mini=w;
    if(d.overload){st.textContent=t('overload');st.className='err'}
    else if(d.stable){st.textContent=t('stable');st.className='ok'}
    else{st.textContent='';st.className=''}}
  $('weight').textContent=w;$('mini').textContent=mini;
  $('rawnet').textContent=d.valid?d.raw_net:'-';
  const cs=$('calstate');
  if(!d.valid){cs.textContent=t('no_sensor',d.sensor||'Sensor');cs.className='val err'}
  else if(d.calibrated){cs.textContent=t('calibrated');cs.className='val ok'}
  else{cs.textContent=t('not_calibrated');cs.className='val warn'}
  $('wifishort').textContent=d.wifi?t('wifi_connected')+(d.ssid?' ('+d.ssid+')':''):t('wifi_disconnected');$('wifishort').className='val '+(d.wifi?'ok':'err');
  $('tare').style.display=d.tare?'':'none';$('tare2').style.display=d.tare?'':'none';
  SIMPLE=!!d.simple;$('msimple').checked=SIMPLE;$('msmart').checked=!SIMPLE;
  if(SIMPLE){$('smshort').textContent=t('sm_off');$('smshort').className='val'}
  if(d.ip!==undefined){
    $('ssid').textContent=d.ssid;$('ip').textContent=d.ip;$('rssi').textContent=d.rssi+' dBm';
    $('up').textContent=dur(d.uptime_s);
    $('heap').textContent=Math.round(d.heap_free/1024)+' KB / '+Math.round(d.psram_free/1024)+' KB';
    $('slot').textContent=d.slot;
    $('fwver').textContent=d.fw_version+(d.fw_update?' → '+d.fw_update:'');
    document.querySelector('#menu a[data-page=system]').classList.toggle('upd',!!d.fw_update)}
  $('curuser').textContent=(ME&&ME.name)||t('nobody');$('devuser').textContent=t('at_device')+': '+(d.user||t('nobody'));
  if(ME&&d.catalog!==undefined&&!!ME.catalog!==!!d.catalog){ME.catalog=d.catalog;document.body.dataset.cat=d.catalog?'1':'';if(!d.catalog&&location.hash==='#catalog')location.hash='#overview'}
  const n=d.nfc,nf=$('nfcshort'),K={spool:'k_spool',box:'k_box',user:'k_user',admin:'k_admin',unknown:'k_unknown'};
  if(!n.module){nf.textContent=t('nfc_none');nf.className='val err'}
  else if(n.present){nf.textContent=t(K[n.kind]||'k_unknown')+' ('+n.uid+')';nf.className='val'}
  else{nf.textContent=t('nfc_ready');nf.className='val ok'}
  if(d.role==='admin'&&document.activeElement!==$('lang'))$('lang').value=d.lang||LANG;
  $('dot').className='on';
}
async function poll(){
  if(!ME)return;
  try{render(await(await api('/api/status')).json())}
  catch(e){$('dot').className='';$('state').textContent=t('no_connection');$('state').className='err'}
  setTimeout(poll,500)
}
$('tare').onclick=$('tare2').onclick=async()=>{try{await post('/api/tare','');say(t('tare_done'),'ok')}catch(e){say(t('tare_fail',e.message),'err')}};
/* ---- Wägesensor ---- */
const SROLES={hx711:['SCK','DOUT'],nau7802:['SCL','SDA'],ads1232:['SCLK','DOUT','PDWN']};
let SENS=null;
function sensorPins(type,pins){
  const box=$('spins');box.innerHTML='';
  SROLES[type].forEach((role,i)=>{
    const l=document.createElement('label');l.className='row';l.style.marginRight='12px';
    const sp=document.createElement('span');sp.textContent=role;l.appendChild(sp);
    const sel=document.createElement('select');sel.dataset.idx=i;
    const opts=[];if(i===2)opts.push([-1,t('sensor_none')]);
    if(type==='nau7802')opts.push([i===0?47:48,'GPIO'+(i===0?47:48)+' ('+t('sensor_bus')+')']);
    SENS.free.forEach(p=>opts.push([p,'GPIO'+p]));
    opts.forEach(([v,txt])=>{const o=document.createElement('option');o.value=v;o.textContent=txt;sel.appendChild(o)});
    sel.value=pins[i];l.appendChild(sel);box.appendChild(l)});
  $('shint').textContent=t('sh_'+type);
}
async function loadSensor(){try{SENS=await(await api('/api/sensor')).json();$('stype').value=SENS.stored.type;sensorPins(SENS.stored.type,SENS.stored.pins);$('srestart').style.display=SENS.restart?'':'none'}catch(e){say(t('load_fail',e.message),'err')}}
$('stype').onchange=()=>{const ty=$('stype').value;sensorPins(ty,ty===SENS.stored.type?SENS.stored.pins:SENS.defaults[ty])};
$('ssave').onclick=async()=>{const ty=$('stype').value,pins=[-1,-1,-1];
  document.querySelectorAll('#spins select').forEach(s=>{pins[+s.dataset.idx]=+s.value});
  if(!confirm(t('sensor_confirm',$('stype').selectedOptions[0].textContent)))return;
  try{await post('/api/sensor',JSON.stringify({type:ty,pins,reboot:true}));say(t('rebooting'),'ok')}catch(e){say(e.message,'err')}};
$('cal').onclick=async()=>{try{await post('/api/calibrate?grams='+encodeURIComponent($('ref').value),'');say(t('cal_done'),'ok')}catch(e){say(t('cal_fail',e.message),'err')}};

/* ---- Spoolman ---- */
let smUrlFilled=false,SIMPLE=false;
function lookupText(l){
  if(l.state==='found'){const s=l.spool;return '#'+s.id+' '+[s.vendor,s.material].filter(Boolean).join(' ')+(s.remaining>=0?', '+Math.round(s.remaining)+' g':'')+(s.location?', '+s.location:'')+(s.owner?', '+t('owner')+' '+s.owner:'')}
  if(l.state==='not_found')return t('scan_unknown',l.uid);
  if(l.state==='error')return t('scan_error',l.error);
  return '-';
}
async function pollSpoolman(){
  if(!ME||ME.role!=='admin')return;
  try{const d=await(await api('/api/spoolman')).json();
    if(!smUrlFilled){$('smurl').value=d.url;smUrlFilled=true}
    const st=$('smstate'),sh=$('smshort'),q=d.queue.count;
    if(SIMPLE){st.textContent=t('sm_off');st.className='';sh.textContent=t('sm_off');sh.className='val'}
    else if(!d.checked){st.textContent=t('checking');st.className='';sh.textContent='...';sh.className='val'}
    else if(d.reachable){st.textContent=t('reachable',d.version);st.className='ok';sh.textContent=t('version',d.version)+(q?' · '+t('pending',q):'');sh.className='val ok'}
    else{st.textContent=t('unreachable');st.className='err';sh.textContent=t('unreach_short')+(q?' · '+t('pending',q):'');sh.className='val err'}
    const lt=lookupText(d.lookup);$('lastscan1').textContent=lt;$('lastscan2').textContent=lt;
    const qi=$('queueinfo');qi.textContent=t('queue_info',q)+(d.queue.dropped?t('queue_dropped',d.queue.dropped):'');qi.className='val'+(q?' warn':'')}catch(e){}
  setTimeout(pollSpoolman,3000)
}
$('smsave').onclick=async()=>{try{await post('/api/spoolman',JSON.stringify({url:$('smurl').value}));say(t('url_saved'),'ok')}catch(e){say(t('save_fail',e.message),'err')}};
$('smcheck').onclick=async()=>{try{await post('/api/spoolman/check','');say(t('checking_sm'))}catch(e){say(e.message,'err')}};
$('qflush').onclick=async()=>{try{await post('/api/spoolman/queue/flush','');say(t('checking_sm'))}catch(e){say(e.message,'err')}};
$('qclear').onclick=async()=>{if(!confirm(t('confirm_clear_queue')))return;try{await post('/api/spoolman/queue/clear','');say('OK','ok')}catch(e){say(e.message,'err')}};

/* ---- Benutzer ---- */
let users=[];
function renderUsers(){
  const box=$('users');box.innerHTML='';
  if(!users.length){box.innerHTML='<div style="color:var(--dim)"></div>';box.firstChild.textContent=t('no_users');return}
  users.forEach(u=>{
    const r=document.createElement('div');r.className='item';
    r.innerHTML='<span class="name"></span><label class="tarebox"><input type="checkbox"><span></span></label><label class="tarebox catbox"><input type="checkbox"><span></span></label><label class="tarebox wobox"><input type="checkbox"><span></span></label><select></select><button class="sec pin"></button><button></button><button class="sec">&times;</button>';
    const tb=r.querySelector('.tarebox');tb.title=t('tare_right');tb.querySelector('span').textContent=t('tare');
    const cb=tb.querySelector('input');cb.checked=!!u.tare;
    cb.onchange=async()=>{try{users=await(await post('/api/users/tare?id='+u.id+'&allow='+(cb.checked?1:0),'')).json();renderUsers()}catch(e){say(t('err',e.message),'err');renderUsers()}};
    const cbx=r.querySelector('.catbox');cbx.title=t('cat_right');cbx.querySelector('span').textContent=t('filament');
    const cc=cbx.querySelector('input');cc.checked=!!u.catalog;
    cc.onchange=async()=>{try{users=await(await post('/api/users/catalog?id='+u.id+'&allow='+(cc.checked?1:0),'')).json();renderUsers()}catch(e){say(t('err',e.message),'err');renderUsers()}};
    const wbx=r.querySelector('.wobox');wbx.title=t('weigh_only_right');wbx.querySelector('span').textContent=t('weigh_only');
    const wc=wbx.querySelector('input');wc.checked=!!u.weigh_only;
    wc.onchange=async()=>{try{users=await(await post('/api/users/weigh?id='+u.id+'&allow='+(wc.checked?1:0),'')).json();renderUsers()}catch(e){say(t('err',e.message),'err');renderUsers()}};
    r.querySelector('.name').textContent=u.name;
    const sel=r.querySelector('select');
    [['',t('lang_device')],['de','Deutsch'],['en','English']].forEach(([v,l])=>{const o=document.createElement('option');o.value=v;o.textContent=l;sel.appendChild(o)});
    sel.value=u.lang||'';
    sel.onchange=async()=>{try{users=await(await post('/api/users/lang?id='+u.id+'&lang='+sel.value,'')).json();renderUsers()}catch(e){say(t('err',e.message),'err');renderUsers()}};
    const [pb,w,d]=r.querySelectorAll('button');w.textContent=t('write_tag');d.title=t('delete');
    pb.textContent=(u.pin?'\u25CF ':'')+t(u.pin?'pin_change':'pin_set');pb.title=u.pin?'':t('no_pin_hint');
    pb.onclick=async()=>{const v=prompt(t('pin_prompt'));if(v===null)return;
      try{users=await(await post('/api/users/pin?id='+u.id+'&pin='+encodeURIComponent(v.trim()),'')).json();renderUsers();say(t('pin_saved'),'ok')}catch(e){say(t('err',e.message),'err')}};
    w.onclick=()=>writeUser(u);
    d.onclick=async()=>{if(!confirm(t('confirm_del_user',u.name)))return;
      try{users=await(await api('/api/users/delete?id='+u.id,{method:'POST'})).json();renderUsers()}catch(e){say(e.message,'err')}};
    box.appendChild(r)});
}
async function loadUsers(){try{users=await(await api('/api/users')).json();renderUsers()}catch(e){say(t('load_fail',e.message),'err')}}
$('adduser').onclick=async()=>{const name=$('newuser').value.trim();if(!name)return;
  try{users=await(await post('/api/users',JSON.stringify({name,pin:$('newpin').value.trim()}))).json();$('newuser').value='';$('newpin').value='';renderUsers();say(t('user_created'),'ok')}catch(e){say(t('err',e.message),'err')}};
let admin={lock:true,default_password:false};
async function loadAdmin(){try{admin=await(await api('/api/admin')).json();$('admlock').checked=admin.lock;$('genon').checked=!!admin.generic;$('gensetpin').textContent=(admin.generic_pin?'\u25CF ':'')+t('set_pin');$('admwarn').style.display=(admin.default_password&&admin.lock)?'':'none';$('admpinwarn').style.display=(admin.default_pin&&admin.lock)?'':'none'}catch(e){}}
$('admlock').onchange=async e=>{const on=e.target.checked;
  if(!on&&!confirm(t('confirm_unlock'))){e.target.checked=true;return}
  try{await post('/api/admin/lock',JSON.stringify({enabled:on}));say(t(on?'lock_on':'lock_off'),'ok')}catch(err){e.target.checked=!on;say(t('err',err.message),'err')}
  loadAdmin()};
$('admsetpw').onclick=async()=>{const pw=$('admpw').value;if(!pw)return;
  try{await post('/api/admin/password',JSON.stringify({password:pw}));$('admpw').value='';say(t('pw_changed'),'ok')}catch(e){say(t('err',e.message),'err')}
  loadAdmin()};
$('admsetdpin').onclick=async()=>{const v=$('admdpin').value.trim();if(!v)return;try{await post('/api/admin/pin',JSON.stringify({pin:v}));$('admdpin').value='';say(t('dev_pin_saved'),'ok')}catch(e){say(t('err',e.message),'err')}loadAdmin()};
$('admtag').onclick=()=>writeTag('/api/admin/tag',t('admin'));
$('genon').onchange=async e=>{try{await post('/api/generic',JSON.stringify({enabled:e.target.checked}))}catch(err){e.target.checked=!e.target.checked;say(t('err',err.message),'err')}loadAdmin()};
$('gensetpin').onclick=async()=>{try{await post('/api/users/pin?id=0&pin='+encodeURIComponent($('genpin').value.trim()),'');$('genpin').value='';say(t('pin_saved'),'ok')}catch(e){say(t('err',e.message),'err')}loadAdmin()};
const WRITE_CLS={waiting:'',writing:'',ok:'ok'};
const writeUser=u=>writeTag('/api/users/write?id='+u.id,u.name);
async function writeTag(url,label){
  const m=$('writemsg');
  try{await post(url,'')}catch(e){m.textContent=t('err',e.message);m.className='err';return}
  for(let i=0;i<90;i++){
    try{const d=await(await api('/api/nfc/write')).json();
      if(!d.module){m.textContent=t('w_nomodule');m.className='err';return}
      if(d.state!=='idle'){m.textContent=label+': '+t('w_'+d.state);m.className=d.state in WRITE_CLS?WRITE_CLS[d.state]:'err'}
      if(d.state!=='waiting'&&d.state!=='writing'&&d.state!=='idle')return}catch(e){}
    await sleep(500)}
}

/* ---- Katalog ---- */
let cat={materials:[],manufacturers:[],colors:[]};
function renderCat(){
  const m=$('mats');m.innerHTML='';
  cat.materials.forEach((x,i)=>{
    const r=document.createElement('div');r.className='item';
    r.innerHTML='<input maxlength="15"><input class="n" type="number" min="0" max="500"><input class="n" type="number" min="0" max="500"><button class="sec">&times;</button>';
    const [a,b,c]=r.querySelectorAll('input');a.value=x.name;b.value=x.min_temp;c.value=x.max_temp;
    a.placeholder=t('ph_type');b.placeholder=t('ph_min');c.placeholder=t('ph_max');r.querySelector('button').title=t('remove');
    a.oninput=()=>{x.name=a.value};b.oninput=()=>{x.min_temp=+b.value};c.oninput=()=>{x.max_temp=+c.value};
    r.querySelector('button').onclick=()=>{cat.materials.splice(i,1);renderCat()};
    m.appendChild(r)});
  const k=$('makers');k.innerHTML='';
  cat.manufacturers.forEach((name,i)=>{
    const r=document.createElement('div');r.className='item';
    r.innerHTML='<input maxlength="31"><button class="sec">&times;</button>';
    const a=r.querySelector('input');a.value=name;a.placeholder=t('ph_maker');a.oninput=()=>{cat.manufacturers[i]=a.value};
    r.querySelector('button').title=t('remove');
    r.querySelector('button').onclick=()=>{cat.manufacturers.splice(i,1);renderCat()};
    k.appendChild(r)});
  const co=$('colors');co.innerHTML='';
  cat.colors.forEach((x,i)=>{
    const r=document.createElement('div');r.className='item';
    r.innerHTML='<input type="color"><input maxlength="15"><button class="sec">&times;</button>';
    const [pick,a]=r.querySelectorAll('input');pick.value='#'+x.hex;a.value=x.name;a.placeholder=t('ph_color');
    pick.oninput=()=>{x.hex=pick.value.slice(1).toUpperCase()};a.oninput=()=>{x.name=a.value};
    r.querySelector('button').title=t('remove');
    r.querySelector('button').onclick=()=>{cat.colors.splice(i,1);renderCat()};
    co.appendChild(r)});
}
let syncTimer=null;
function syncText(d){
  if(d.state==='busy')return t('sync_running');
  if(d.state==='error')return t('sync_err',d.error);
  if(d.state==='ok')return t('sync_ok',d.added_makers,d.added_materials,d.vendors_read,d.materials_read)+(d.skipped?t('sync_skip',d.skipped):'')+t('sync_ago',d.seconds_ago);
  return t('sync_never');
}
async function loadSync(){
  clearTimeout(syncTimer);
  try{const d=await(await api('/api/catalog/sync')).json();$('syncinfo').textContent=syncText(d);
    if(d.state==='busy')syncTimer=setTimeout(async()=>{await loadSync();if(location.hash==='#catalog')loadCat()},1500)}catch(e){}
}
$('syncsm').onclick=async()=>{try{await post('/api/catalog/sync','');$('syncinfo').textContent=t('sync_running');setTimeout(loadSync,1000)}catch(e){say(t('err',e.message),'err')}};
async function loadCat(){try{cat=await(await api('/api/catalog')).json();renderCat()}catch(e){say(t('load_fail',e.message),'err')}}
$('addmat').onclick=()=>{cat.materials.push({name:'',min_temp:200,max_temp:230});renderCat()};
$('addmaker').onclick=()=>{cat.manufacturers.push('');renderCat()};
$('addcolor').onclick=()=>{cat.colors.push({name:'',hex:'FFFFFF'});renderCat()};
$('loadcat').onclick=loadCat;
$('savecat').onclick=async()=>{try{await post('/api/catalog',JSON.stringify(cat));say(t('catalog_saved'),'ok');loadCat()}catch(e){say(t('save_fail',e.message),'err')}};

/* ---- WLAN ---- */
async function loadWifi(){
  try{const d=await(await api('/api/wifi')).json();
    $('w_ssid').textContent=d.connected?d.ssid:'-';
    $('w_source').textContent=d.source==='nvs'?t('src_nvs')+(d.saved?' ('+d.saved+')':''):t('src_none');
    $('w_ap').textContent=d.ap_active?t('ap_active',d.ap_ssid,d.ap_ip):t('ap_inactive');
    if(!$('wssid').value&&d.saved)$('wssid').value=d.saved}catch(e){}
}
$('apon').onclick=async()=>{try{await post('/api/wifi/ap?on=1','');await sleep(800);loadWifi()}catch(e){say(e.message,'err')}};
$('apoff').onclick=async()=>{try{await post('/api/wifi/ap?on=0','');await sleep(800);loadWifi()}catch(e){say(e.message,'err')}};
$('scan').onclick=async()=>{
  $('scanmsg').textContent=t('scanning');$('nets').innerHTML='';
  try{const list=await(await api('/api/wifi/scan')).json();$('scanmsg').textContent=list.length?'':t('no_networks');
    list.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{
      const r=document.createElement('div');r.className='item net';
      r.innerHTML='<span class="name"></span><span style="color:var(--dim)"></span>';
      r.querySelector('.name').textContent=(n.open?'':'🔒 ')+n.ssid;r.lastChild.textContent=n.rssi+' dBm';
      r.onclick=()=>{$('wssid').value=n.ssid;$('wpass').focus()};
      $('nets').appendChild(r)})}catch(e){$('scanmsg').textContent=t('err',e.message)}
};
$('wsave').onclick=async()=>{const ssid=$('wssid').value.trim();if(!ssid)return say(t('ssid_missing'),'err');
  try{await post('/api/wifi',JSON.stringify({ssid,password:$('wpass').value}));say(t('wifi_saved'),'ok')}catch(e){say(t('save_fail',e.message),'err')}};
$('wreset').onclick=async()=>{if(!confirm(t('confirm_wifi_reset')))return;try{await post('/api/wifi/reset','');say(t('wifi_saved'),'ok')}catch(e){say(e.message,'err')}};

/* ---- Log ---- */
let logSeq=0,logPaused=false,logTimer=null;
const logBox=()=>$('logbox');
function logPauseLabel(){$('logpause').textContent=t(logPaused?'resume':'pause')}
async function pollLog(){
  if(!logPaused){
    try{const r=await api('/api/log?since='+logSeq);const text=await r.text();const seq=+r.headers.get('X-Log-Seq');
      if(text){const b=logBox(),atEnd=b.scrollTop+b.clientHeight>=b.scrollHeight-30;
        b.textContent+=text;
        const lines=b.textContent.split('\n');if(lines.length>1500)b.textContent=lines.slice(-1200).join('\n');
        if(atEnd)b.scrollTop=b.scrollHeight}
      if(seq>logSeq)logSeq=seq}catch(e){}
  }
  logTimer=setTimeout(pollLog,1000)
}
function startLog(){if(!logTimer)pollLog()}
$('logpause').onclick=()=>{logPaused=!logPaused;logPauseLabel()};
$('logclear').onclick=()=>{logBox().textContent=''};
$('logdl').onclick=()=>{const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([logBox().textContent],{type:'text/plain'}));a.download='filamentwaage-log.txt';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),2000)};

/* ---- System ---- */
async function loadTheme(){try{$('theme').value=await(await api('/api/theme')).text()}catch(e){say(t('load_fail',e.message),'err')}}
$('load').onclick=loadTheme;
$('save').onclick=async()=>{try{JSON.parse($('theme').value)}catch(e){return say(t('invalid_json',e.message),'err')}
  try{await post('/api/theme',$('theme').value);say(t('saved_restarting'),'ok')}catch(e){say(t('save_fail',e.message),'err')}};
$('reboot').onclick=async()=>{if(!confirm(t('confirm_reboot')))return;try{await post('/api/reboot','');say(t('rebooting'),'ok')}catch(e){say(e.message,'err')}};
/* ---- Selbstdiagnose ---- */
const DIAG_MARK={ok:'✓',info:'…',warn:'!',error:'✕'},DIAG_CLS={ok:'ok',info:'',warn:'warn',error:'err'};
let diagTimer=null;
function diagRender(d){
  const box=$('diag');box.textContent='';
  d.items.forEach(i=>{
    const k='dg_'+i.comp+'_'+i.code,row=document.createElement('div');row.style.margin='0 0 12px';
    const h=document.createElement('div');h.className=DIAG_CLS[i.level];
    const b=document.createElement('b');b.textContent=DIAG_MARK[i.level]+' '+t('dg_c_'+i.comp)+': ';
    h.appendChild(b);h.appendChild(document.createTextNode(t(k,d.http)));row.appendChild(h);
    const tk=[k+'_tip_'+d.sensor,k+'_tip'].find(x=>I18N[LANG][x]!==undefined||I18N.de[x]!==undefined);
    if(tk){const p=document.createElement('div');p.className='hint';p.style.margin='4px 0 0';p.textContent=t(tk);row.appendChild(p)}
    box.appendChild(row)});
}
async function pollDiag(){
  if(!ME||ME.role!=='admin'||location.hash!=='#system'){diagTimer=null;return}
  try{diagRender(await(await api('/api/diag')).json())}catch(e){}
  diagTimer=setTimeout(pollDiag,3000)
}
function startDiag(){if(!diagTimer)pollDiag()}
/* ---- Firmware-Update ---- */
let fwTimer=null,fwPrev='',fwWaitVer='',fwLast=null;
function fwRender(d){
  fwLast=d;
  $('fwcur').textContent=d.version;$('fwlatest').textContent=d.latest?d.latest+(d.latest_channel!=='stable'?' ('+t('ch_'+d.latest_channel)+')':''):'-';
  if(document.activeElement!==$('fwchan'))$('fwchan').value=d.channel;
  const m=$('fwmsg'),bar=$('fwbar'),busy=d.state==='checking'||d.state==='downloading'||d.state==='done';
  let txt,cls='';
  if(d.state==='checking')txt=t('fw_checking');
  else if(d.state==='downloading'){const p=d.total?Math.floor(d.done*100/d.total):0;bar.value=p;txt=t('fw_dl',p);cls='warn'}
  else if(d.state==='done'){bar.value=100;txt=t('fw_done');cls='ok'}
  else if(d.state==='error'){txt=d.error;cls='err'}
  else if(!d.checked)txt=t('fw_never');
  else if(d.no_release)txt=t('fw_norel');
  else if(d.available){txt=t(/^v?\d+\.\d+/.test(d.version)?'fw_avail':'fw_unknown',d.latest);cls='ok'}
  else if(d.downgrade){txt=t('fw_down_info',d.latest);cls='warn'}
  else{txt=t('fw_uptodate');cls='ok'}
  if(d.checked&&!busy&&d.checked_s!==undefined)txt+=' ('+t('fw_ago',dur(d.checked_s))+')';
  m.textContent=txt;m.className=cls;
  bar.style.display=d.state==='downloading'||d.state==='done'?'':'none';
  $('fwcheck').disabled=busy;$('fwchan').disabled=busy;
  $('fwdown').style.display=d.downgrade&&!busy?'':'none';$('fwdown').textContent=t('fw_down',d.latest);
  $('fwinstall').style.display=d.available&&!busy?'':'none';
  $('fwinstall').dataset.tag=d.latest||'';
  $('fwpage').style.display=d.page_url?'':'none';if(d.page_url)$('fwpage').href=d.page_url;
  if(d.notes!==undefined){const n=$('fwnotes');n.textContent=d.notes+(d.notes_cut?'\n'+t('fw_cut'):'');n.style.display=d.notes&&d.latest?'':'none'}
}
async function loadFw(notes){
  clearTimeout(fwTimer);fwTimer=null;
  let d;
  try{d=await(await api('/api/update'+(notes?'?notes=1':''))).json()}
  catch(e){if(fwWaitVer&&ME){fwTimer=setTimeout(()=>loadFw(true),2000)}return}  // während des Neustarts nicht erreichbar; nach Abmeldung (401) nicht weiter fragen
  if(fwWaitVer&&d.state==='idle'){say(t('fw_back',d.version),'ok');fwWaitVer=''}
  const changed=fwPrev!==d.state;fwPrev=d.state;
  fwRender(d);
  if(d.state==='checking'||d.state==='downloading')fwTimer=setTimeout(()=>loadFw(false),1000);
  else if(d.state==='done'){fwWaitVer=d.latest;fwTimer=setTimeout(()=>loadFw(true),5000)}
  else if(changed&&!notes)loadFw(true);  // Prüfung fertig: Release Notes holen
}
$('fwcheck').onclick=async()=>{try{await post('/api/update/check','');fwPrev='';loadFw(false)}catch(e){say(e.message,'err')}};
async function fwInstall(q){if(!confirm(q))return;
  try{await post('/api/update/install','');fwPrev='';loadFw(false)}catch(e){say(e.message,'err')}}
$('fwinstall').onclick=()=>fwInstall(t('fw_confirm',$('fwinstall').dataset.tag));
$('fwdown').onclick=()=>fwInstall(t('fw_down_confirm',$('fwinstall').dataset.tag));
$('fwchan').onchange=async()=>{try{await post('/api/update/channel?ch='+$('fwchan').value,'');say(t('fw_chan_set'),'ok');$('fwnotes').style.display='none';fwPrev='';loadFw(false)}catch(e){say(e.message,'err');loadFw(true)}};
$('touchreset').onclick=async()=>{if(!confirm(t('confirm_touch')))return;try{await post('/api/touch/reset','');say(t('touch_done'),'ok')}catch(e){say(e.message,'err')}};
$('lang').onchange=async()=>{
  const l=$('lang').value;
  try{await post('/api/language?lang='+l,'');applyLang(l);say(t('lang_set'),'ok')}catch(e){say(t('save_fail',e.message),'err')}};
async function loadSS(){try{const d=await(await api('/api/screensaver')).json();$('ssdim').value=d.dim_min;$('ssoff').value=d.off_min}catch(e){}}
$('msmart').onchange=$('msimple').onchange=async()=>{const simple=$('msimple').checked;
  try{await post('/api/mode',JSON.stringify({simple}));SIMPLE=simple;say(t('mode_set'),'ok')}catch(e){say(t('save_fail',e.message),'err')}};
$('sssave').onclick=async()=>{try{await post('/api/screensaver?dim='+(+$('ssdim').value||0)+'&off='+(+$('ssoff').value||0),'');say(t('ss_saved'),'ok')}catch(e){say(t('save_fail',e.message),'err')}};

/* ---- Anmeldung ---- */
let ME=null,polling=false;
function showLogin(info){
  ME=null;polling=false;document.body.dataset.role='none';document.body.dataset.cat='';
  $('wrap').style.display='none';$('login').style.display='flex';
  $('lsecret').value='';
  if(info){$('lgen').style.display=info.generic?'':'none';$('lgen').dataset.pin=info.generic_pin?'1':''}
  else boot();
}
function setRole(me){
  ME=me;document.body.dataset.role=me.role;document.body.dataset.cat=me.catalog?'1':'';
  $('login').style.display='none';$('wrap').style.display='';$('lerr').textContent='';
  $('whoami').textContent=me.name||'';
  $('mylang').value=me.lang||'';
  if(me.role!=='admin'&&location.hash!==''&&location.hash!=='#overview'&&!(location.hash==='#catalog'&&me.catalog))location.hash='#overview';
  show();
  if(!polling){polling=true;poll();if(me.role==='admin')pollSpoolman()}
}
async function boot(){
  try{const me=await(await fetch('/api/me')).json();
    if(me.role==='none')showLogin(me);else setRole(me)}
  catch(e){setTimeout(boot,2000)}
}
async function doLogin(name,secret){
  $('lerr').textContent='';
  try{const r=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'text/plain'},body:JSON.stringify({name,secret})});
    if(!r.ok)throw new Error((await r.text())||('HTTP '+r.status));
    loaded.users=loaded.catalog=loaded.system=loaded.wifi=loaded.log=false;
    setRole(await r.json())}
  catch(e){$('lerr').textContent=e.message}
}
$('lgo').onclick=()=>doLogin($('lname').value,$('lsecret').value);
$('lgen').onclick=()=>doLogin('generic',$('lsecret').value);
['lname','lsecret'].forEach(i=>$(i).addEventListener('keydown',e=>{if(e.key==='Enter')$('lgo').click()}));
$('logout').onclick=async()=>{try{await post('/api/logout','')}catch(e){}ME=null;polling=false;showLogin();};
$('mylang').onchange=async()=>{try{await post('/api/me/lang?lang='+$('mylang').value,'')}catch(e){say(t('err',e.message),'err')}};
$('changepin').onclick=async()=>{try{await post('/api/me/pin',JSON.stringify({old:$('oldpin').value,pin:$('newpin2').value}));$('oldpin').value=$('newpin2').value='';say(t('pin_changed'),'ok')}catch(e){say(t('err',e.message),'err')}};
applyLang(navigator.language&&navigator.language.toLowerCase().startsWith('en')?'en':'de');boot();
</script></body></html>)HTML";
