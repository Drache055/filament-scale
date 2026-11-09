#pragma once

// Weboberfläche (eine Seite, komplett eingebettet), zweisprachig (Deutsch/Englisch, folgt der Gerätesprache).
// Unterseiten per #hash, Daten per Polling von /api/status, /api/spoolman und /api/log.
static const char WEB_PAGE[] = R"HTML(<!doctype html>
<html lang="de"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Filamentwaage</title>
<style>
:root{--bg:#0f1219;--side:#151a24;--card:#1b2130;--text:#f1f5f9;--dim:#8a95a8;--pri:#3b82f6;--ok:#22c55e;--warn:#f59e0b;--err:#ef4444;--line:#2a3147}
@media (prefers-color-scheme:light){:root{--bg:#f3f5f9;--side:#fff;--card:#fff;--text:#0f172a;--dim:#64748b;--line:#dbe1ec}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.45 system-ui,sans-serif}
.app{display:grid;grid-template-columns:230px 1fr;min-height:100vh}
aside{background:var(--side);border-right:1px solid var(--line);padding:18px 12px;position:sticky;top:0;height:100vh;overflow-y:auto}
.brand{font-weight:700;font-size:18px;padding:4px 10px 16px;display:flex;align-items:center;gap:8px}
#dot{width:10px;height:10px;border-radius:50%;background:var(--err)}
#dot.on{background:var(--ok)}
#menu{display:flex;flex-direction:column;gap:2px}
#menu a{color:var(--dim);text-decoration:none;padding:10px 12px;border-radius:10px;font-weight:500}
#menu a:hover{background:var(--bg);color:var(--text)}
#menu a.active{background:var(--pri);color:#fff}
main{padding:24px;max-width:860px;width:100%}
.top{display:flex;align-items:baseline;justify-content:space-between;gap:12px;margin-bottom:18px}
h1{font-size:24px;margin:0}
#mini{color:var(--dim);font-variant-numeric:tabular-nums}
.page{display:none}.page.active{display:block}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:18px;margin-bottom:14px}
.label{font-size:12px;color:var(--dim);text-transform:uppercase;letter-spacing:.06em;margin-bottom:8px}
.val{font-size:18px;font-weight:600;word-break:break-word}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:14px;margin-bottom:14px}
.grid .card{margin:0}.grid .wide{grid-column:1/-1}
#weight{font-size:64px;font-weight:700;font-variant-numeric:tabular-nums;line-height:1.1}
#state{min-height:1.4em;color:var(--dim);margin-bottom:6px}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
button{background:var(--pri);color:#fff;border:0;border-radius:10px;padding:11px 16px;font:inherit;font-weight:500;cursor:pointer}
button:hover{filter:brightness(1.1)}
button.sec{background:transparent;color:var(--text);border:1px solid var(--line)}
input,textarea,select{background:transparent;color:var(--text);border:1px solid var(--line);border-radius:10px;padding:10px;font:inherit;max-width:100%}
select option{color:#000}
input[type=number]{width:7em}
input[type=color]{width:3em;padding:2px;height:2.6em;flex:none}
textarea{width:100%;height:300px;font:13px/1.45 ui-monospace,monospace}
table{width:100%;border-collapse:collapse}
td{padding:7px 0;border-bottom:1px solid var(--line)}tr:last-child td{border-bottom:0}
td:last-child{text-align:right;font-variant-numeric:tabular-nums}
p.hint{color:var(--dim);margin:0 0 12px}
ol.steps{margin:0 0 14px;padding-left:20px;color:var(--dim)}ol.steps li{margin-bottom:4px}
.item{display:flex;gap:8px;align-items:center;margin-bottom:8px}
.item .name{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis}
.item input[type=text],.item input:not([type]){flex:1;min-width:0}.item input.n{flex:none;width:5em}
.item button{padding:8px 12px}
.item select{padding:8px 6px;max-width:9em}
.item .tarebox{display:flex;gap:4px;align-items:center;font-size:.9em}
body:not([data-role=admin]) .admin-only{display:none!important}
body:not([data-role=user]) .user-only{display:none!important}
body:not([data-role=admin]):not([data-cat="1"]) .cat-only{display:none!important}
#login{position:fixed;inset:0;display:none;align-items:center;justify-content:center;padding:16px;background:var(--bg);z-index:20}
#login .box{width:100%;max-width:360px}
#login h1{margin-bottom:14px}
#login input{width:100%;margin-bottom:10px}
#login .row button{flex:1}
#whoami{color:var(--dim);margin-right:6px}
.item{flex-wrap:wrap}
.net{cursor:pointer}.net:hover{background:var(--bg)}
h3{font-size:15px;margin:18px 0 8px}
#logbox{background:#0a0d13;color:#cbd5e1;border-radius:10px;padding:12px;height:62vh;overflow:auto;font:12px/1.45 ui-monospace,monospace;white-space:pre-wrap;word-break:break-all;margin:0}
.ok{color:var(--ok)}.warn{color:var(--warn)}.err{color:var(--err)}
#toast{position:fixed;left:50%;bottom:22px;transform:translateX(-50%) translateY(30px);background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px 18px;box-shadow:0 8px 30px #0006;opacity:0;pointer-events:none;transition:.25s;max-width:92vw;z-index:9}
#toast.show{opacity:1;transform:translateX(-50%)}
@media (max-width:760px){
  .app{display:block}
  aside{position:sticky;top:0;z-index:5;height:auto;padding:8px 10px;border-right:0;border-bottom:1px solid var(--line);overflow:visible}
  .brand{padding:0 4px 8px}
  #menu{flex-direction:row;overflow-x:auto;gap:4px;padding-bottom:2px}
  #menu a{white-space:nowrap;padding:8px 12px}
  main{padding:16px}
  #weight{font-size:52px}
  #logbox{height:55vh}
}
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
no_hx711:'HX711 nicht gefunden',not_calibrated:'Nicht kalibriert',calibrated:'Kalibriert',overload:'Überlast',stable:'stabil',no_connection:'Keine Verbindung',
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
language:'Sprache',lang_hint:'Gilt für die Anzeige am Gerät (es springt dabei zum Startbildschirm) und für diese Webseite.',lang_set:'Sprache auf Deutsch gestellt',
screensaver:'Bildschirmschoner',ss_hint:'Nach der eingestellten Zeit ohne Berührung, Tag oder Gewichtsänderung wird die Anzeige gedimmt, später ausgeschaltet. 0 = nie. Der erste Tipp weckt nur auf.',dim_after:'Dimmen nach',off_after:'Aus nach',ss_saved:'Bildschirmschoner gespeichert',
theme_hint:'Farben, Radius, Abstände und Schriftgrößen der Anzeige am Gerät. Nach dem Speichern startet das Gerät neu.',reload:'Neu laden',maintenance:'Wartung',fw_slot:'Firmware-Slot',reboot:'Gerät neu starten',touch_reset:'Touch-Kalibrierung zurücksetzen',
saved_restarting:'Gespeichert, Gerät startet neu ...',confirm_reboot:'Gerät neu starten?',rebooting:'Neustart ...',confirm_touch:'Touch-Kalibrierung zurücksetzen? Danach am Gerät neu kalibrieren (Menü → Einstellungen → Touch kalibrieren).',touch_done:'Touch-Kalibrierung zurückgesetzt',
tare_done:'Tara gesetzt',tare_fail:'Tara fehlgeschlagen: {0}',cal_done:'Kalibriert',cal_fail:'Kalibrierung fehlgeschlagen: {0}',
nfc_none:'Modul nicht gefunden',nfc_ready:'bereit',k_spool:'Spule',k_box:'Box',k_user:'Benutzer',k_unknown:'Tag ohne Format',
w_waiting:'Tag an die Waage halten (vorher entfernen und neu auflegen) ...',w_writing:'Schreibe ...',w_ok:'Tag geschrieben',w_locked:'Tag ist gesperrt',w_too_big:'Passt nicht auf den Tag',w_not_ntag:'Kein NTAG erkannt',w_failed:'Schreiben fehlgeschlagen',w_timeout:'Kein Tag erkannt, Zeit abgelaufen',w_occupied:'Der Tag enthält eine Spule und wurde nicht überschrieben',w_nomodule:'NFC-Modul nicht gefunden',
err:'Fehler: {0}',load_fail:'Laden fehlgeschlagen: {0}',save_fail:'Speichern fehlgeschlagen: {0}',invalid_json:'Kein gültiges JSON: {0}',checking_sm:'Prüfe Spoolman ...'
},
en:{
m_overview:'Overview',m_calibration:'Calibration',m_users:'Users',m_catalog:'Filament catalog',m_spoolman:'Spoolman',m_wifi:'WiFi',m_log:'Log',m_system:'System',
weight:'Weight',tare:'Tare',cur_user:'Logged-in user',nobody:'nobody',last_scan:'Last scanned spool',device:'Device',wlan:'WiFi',ip:'IP address',signal:'Signal',uptime:'Uptime',mem:'Free RAM / PSRAM',
status:'Status',raw_net:'Raw value (net)',procedure:'Procedure',cal1:'Empty the scale and press <b>Tare</b>.',cal2:'Place a weight of known mass (the heavier, the more accurate).',cal3:'Enter the mass and press <b>Calibrate</b>.',calibrate:'Calibrate',
no_hx711:'HX711 not found',not_calibrated:'Not calibrated',calibrated:'Calibrated',overload:'Overload',stable:'stable',no_connection:'No connection',
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
language:'Language',lang_hint:'Applies to the display on the device (it jumps to the home screen) and to this web page.',lang_set:'Language set to English',
screensaver:'Screensaver',ss_hint:'After the set time without touch, tag or weight change the display is dimmed, later switched off. 0 = never. The first tap only wakes it up.',dim_after:'Dim after',off_after:'Off after',ss_saved:'Screensaver saved',
theme_hint:'Colors, radius, spacing and font sizes of the display on the device. The device restarts after saving.',reload:'Reload',maintenance:'Maintenance',fw_slot:'Firmware slot',reboot:'Restart device',touch_reset:'Reset touch calibration',
saved_restarting:'Saved, device restarts ...',confirm_reboot:'Restart the device?',rebooting:'Restarting ...',confirm_touch:'Reset touch calibration? Then recalibrate on the device (Menu → Settings → Calibrate touch).',touch_done:'Touch calibration reset',
tare_done:'Tare set',tare_fail:'Tare failed: {0}',cal_done:'Calibrated',cal_fail:'Calibration failed: {0}',
nfc_none:'Module not found',nfc_ready:'ready',k_spool:'Spool',k_box:'Box',k_user:'User',k_unknown:'Tag without format',
w_waiting:'Hold the tag to the scale (remove and place it again first) ...',w_writing:'Writing ...',w_ok:'Tag written',w_locked:'Tag is locked',w_too_big:'Does not fit on the tag',w_not_ntag:'Not an NTAG',w_failed:'Write failed',w_timeout:'No tag detected, timed out',w_occupied:'The tag holds a spool and was not overwritten',w_nomodule:'NFC module not found',
err:'Error: {0}',load_fail:'Loading failed: {0}',save_fail:'Saving failed: {0}',invalid_json:'Not valid JSON: {0}',checking_sm:'Checking Spoolman ...'
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
  if(changed){renderUsers();renderCat();logPauseLabel()}
}

let toastTimer;
function say(msg,c){const m=$('toast');m.textContent=msg;m.className='show '+(c||'');clearTimeout(toastTimer);toastTimer=setTimeout(()=>{m.className=''},4500)}
async function api(path,opt){const r=await fetch(path,opt);if(!r.ok){if(r.status===401&&ME)showLogin();throw new Error((await r.text())||('HTTP '+r.status))}return r}
const post=(p,body)=>api(p,{method:'POST',headers:{'Content-Type':'text/plain'},body});

/* ---- Menü ---- */
const PAGES=['overview','calibration','users','catalog','spoolman','wifi','log','system'];
const loaded={};
const ONLOAD={users:()=>{loadUsers();loadAdmin()},catalog:()=>{loadCat();loadSync()},system:()=>{loadTheme();loadSS()},wifi:()=>loadWifi(),log:()=>startLog()};
function show(langOnly){
  let p=location.hash.slice(1);if(!PAGES.includes(p)||(p!=='overview'&&!(ME&&(ME.role==='admin'||(p==='catalog'&&ME.catalog)))))p='overview';
  document.querySelectorAll('.page').forEach(e=>e.classList.toggle('active',e.id==='p-'+p));
  document.querySelectorAll('#menu a').forEach(a=>a.classList.toggle('active',a.dataset.page===p));
  $('title').textContent=t('m_'+p);document.title=t('m_'+p)+' – Filamentwaage';
  if(langOnly===true)return;
  if(!loaded[p]){loaded[p]=true;if(ONLOAD[p])ONLOAD[p]()}
  window.scrollTo(0,0);
}
window.addEventListener('hashchange',()=>show());

/* ---- Status ---- */
function dur(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+'d ':'')+h+'h '+m+'m'}
function render(d){
  if(d.lang&&d.lang!==LANG)applyLang(d.lang);
  const st=$('state');let w='-- g',mini='';
  if(!d.valid){st.textContent=t('no_hx711');st.className='err'}
  else if(!d.calibrated){st.textContent=t('not_calibrated');st.className='warn'}
  else{w=(Math.round(d.weight*10)/10).toFixed(1)+' g';mini=w;
    if(d.overload){st.textContent=t('overload');st.className='err'}
    else if(d.stable){st.textContent=t('stable');st.className='ok'}
    else{st.textContent='';st.className=''}}
  $('weight').textContent=w;$('mini').textContent=mini;
  $('rawnet').textContent=d.valid?d.raw_net:'-';
  const cs=$('calstate');
  if(!d.valid){cs.textContent=t('no_hx711');cs.className='val err'}
  else if(d.calibrated){cs.textContent=t('calibrated');cs.className='val ok'}
  else{cs.textContent=t('not_calibrated');cs.className='val warn'}
  $('wifishort').textContent=d.wifi?t('wifi_connected')+(d.ssid?' ('+d.ssid+')':''):t('wifi_disconnected');$('wifishort').className='val '+(d.wifi?'ok':'err');
  $('tare').style.display=d.tare?'':'none';$('tare2').style.display=d.tare?'':'none';
  if(d.ip!==undefined){
    $('ssid').textContent=d.ssid;$('ip').textContent=d.ip;$('rssi').textContent=d.rssi+' dBm';
    $('up').textContent=dur(d.uptime_s);
    $('heap').textContent=Math.round(d.heap_free/1024)+' KB / '+Math.round(d.psram_free/1024)+' KB';
    $('slot').textContent=d.slot}
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
$('cal').onclick=async()=>{try{await post('/api/calibrate?grams='+encodeURIComponent($('ref').value),'');say(t('cal_done'),'ok')}catch(e){say(t('cal_fail',e.message),'err')}};

/* ---- Spoolman ---- */
let smUrlFilled=false;
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
    if(!d.checked){st.textContent=t('checking');st.className='';sh.textContent='...';sh.className='val'}
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
    r.innerHTML='<span class="name"></span><label class="tarebox"><input type="checkbox"><span></span></label><label class="tarebox catbox"><input type="checkbox"><span></span></label><select></select><button class="sec pin"></button><button></button><button class="sec">&times;</button>';
    const tb=r.querySelector('.tarebox');tb.title=t('tare_right');tb.querySelector('span').textContent=t('tare');
    const cb=tb.querySelector('input');cb.checked=!!u.tare;
    cb.onchange=async()=>{try{users=await(await post('/api/users/tare?id='+u.id+'&allow='+(cb.checked?1:0),'')).json();renderUsers()}catch(e){say(t('err',e.message),'err');renderUsers()}};
    const cbx=r.querySelector('.catbox');cbx.title=t('cat_right');cbx.querySelector('span').textContent=t('filament');
    const cc=cbx.querySelector('input');cc.checked=!!u.catalog;
    cc.onchange=async()=>{try{users=await(await post('/api/users/catalog?id='+u.id+'&allow='+(cc.checked?1:0),'')).json();renderUsers()}catch(e){say(t('err',e.message),'err');renderUsers()}};
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
$('touchreset').onclick=async()=>{if(!confirm(t('confirm_touch')))return;try{await post('/api/touch/reset','');say(t('touch_done'),'ok')}catch(e){say(e.message,'err')}};
$('lang').onchange=async()=>{
  const l=$('lang').value;
  try{await post('/api/language?lang='+l,'');applyLang(l);say(t('lang_set'),'ok')}catch(e){say(t('save_fail',e.message),'err')}};
async function loadSS(){try{const d=await(await api('/api/screensaver')).json();$('ssdim').value=d.dim_min;$('ssoff').value=d.off_min}catch(e){}}
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
