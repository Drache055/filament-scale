// Einrichtung per USB (Web Serial): WLAN und Admin-Passwort direkt nach dem Flashen an die Waage schicken.
// Protokoll: Improv-Serial-Rahmen mit eigenen Befehlen, siehe firmware/src/core/usb_setup_core.h.
// Passwörter werden nur an die Waage geschickt, nirgends gespeichert und nicht protokolliert.
(function(){
  var form=document.getElementById('setup-form');
  if(!form)return;
  var st=document.getElementById('setup-status'),scanBtn=document.getElementById('setup-scan'),list=document.getElementById('setup-ssids');
  var go=document.getElementById('setup-go'),unsup=document.getElementById('setup-unsupported');
  if(!('serial' in navigator)){form.hidden=true;unsup.hidden=false;return}

  var CMD_HELLO=0x80,CMD_SETUP=0x81,CMD_SCAN=0x82,HDR=[73,77,80,82,79,86];
  var ERR={1:['Ungültiges Paket','Invalid packet'],3:['Keine Verbindung mit diesem WLAN. Name und Passwort prüfen (nur 2,4 GHz).','Could not connect to this Wi-Fi. Check name and password (2.4 GHz only).'],
    128:['WLAN-Name: 1 bis 32 Zeichen','Wi-Fi name: 1 to 32 characters'],129:['WLAN-Passwort: leer (offenes WLAN) oder 8 bis 63 Zeichen','Wi-Fi password: empty (open network) or 8 to 63 characters'],
    130:['Admin-Passwort: 4 bis 32 Zeichen, kein „:“, nicht „Filament“','Admin password: 4 to 32 characters, no “:”, not “Filament”']};
  function de(){return document.documentElement.lang!=='en'}
  function t(d,e){return de()?d:e}
  function say(msg,cls){st.textContent=msg;st.className='setup-status '+(cls||'')}

  var port=null,reader=null,buf=[],waiters=[];
  function enc(s){return Array.from(new TextEncoder().encode(s))}
  function packet(cmd,strs){
    var d=[cmd,0];strs.forEach(function(s){var b=enc(s);d.push(b.length);d=d.concat(b)});d[1]=d.length-2;
    var p=HDR.concat([1,3,d.length]).concat(d),sum=0;p.forEach(function(b){sum=(sum+b)&255});p.push(sum);p.push(10);
    return new Uint8Array(p);
  }
  function onPacket(type,data){
    var w=waiters.slice();w.forEach(function(f){f(type,data)});
  }
  function feed(b){
    buf.push(b);var n=buf.length;
    if(n<=6){if(b!==HDR[n-1]){buf=b===HDR[0]?[b]:[]}return}
    if(n===7&&b!==1){buf=[];return}
    if(n<9)return;
    var len=buf[8];if(n<10+len)return;
    var sum=0;for(var i=0;i<n-1;i++)sum=(sum+buf[i])&255;
    if(sum===buf[n-1])onPacket(buf[7],buf.slice(9,9+len));
    buf=[];
  }
  function strings(data){
    var out=[],i=2;while(i<data.length){var l=data[i++];out.push(new TextDecoder().decode(new Uint8Array(data.slice(i,i+l))));i+=l}return out;
  }
  async function readLoop(){
    try{for(;;){var r=await reader.read();if(r.done)break;r.value.forEach(feed)}}catch(e){}
  }
  async function open(){
    if(port)return;
    var ports=await navigator.serial.getPorts();
    var p=ports.length===1?ports[0]:await navigator.serial.requestPort();
    try{await p.open({baudRate:115200})}catch(e){
      // Schon offen (z. B. Installationsfenster noch offen) oder Gerät neu angemeldet: neu auswählen lassen
      if(ports.length===1){p=await navigator.serial.requestPort();await p.open({baudRate:115200})}else throw e;
    }
    try{await p.setSignals({dataTerminalReady:false,requestToSend:false})}catch(e){}
    port=p;reader=port.readable.getReader();readLoop();
  }
  async function close(){
    try{if(reader){await reader.cancel();reader.releaseLock()}}catch(e){}
    try{if(port)await port.close()}catch(e){}
    port=null;reader=null;buf=[];
  }
  async function send(bytes){var w=port.writable.getWriter();try{await w.write(bytes)}finally{w.releaseLock()}}
  // Wartet auf ein Paket, für das match() einen Wert != undefined liefert
  function wait(match,ms){
    return new Promise(function(res,rej){
      var f=function(type,data){var v=match(type,data);if(v!==undefined){done();res(v)}};
      var to=setTimeout(function(){done();rej(new Error('timeout'))},ms);
      function done(){clearTimeout(to);waiters=waiters.filter(function(x){return x!==f})}
      waiters.push(f);
    });
  }
  async function hello(){
    // Die Waage startet nach dem Flashen evtl. noch: mehrmals fragen
    for(var i=0;i<20;i++){
      var p=wait(function(type,data){if(type===4&&data[0]===CMD_HELLO)return strings(data)},800);
      await send(packet(CMD_HELLO,[]));
      try{return await p}catch(e){}
    }
    throw new Error('nohello');
  }
  function failText(e){
    if(e&&e.message==='nohello')return t('Die Waage antwortet nicht. Ist das Installationsfenster geschlossen, der richtige Anschluss gewählt und die Firmware aktuell?','The scale does not answer. Is the install dialog closed, the right port selected and the firmware up to date?');
    if(e&&e.name==='NotFoundError')return t('Kein Anschluss gewählt.','No port selected.');
    if(e&&e.name==='InvalidStateError')return t('Der Anschluss ist belegt. Schließe das Installationsfenster und andere serielle Programme.','The port is busy. Close the install dialog and other serial programs.');
    return t('Fehler: ','Error: ')+(e&&e.message||e);
  }

  scanBtn.onclick=async function(){
    scanBtn.disabled=true;say(t('Verbinde mit der Waage …','Connecting to the scale …'));
    try{
      await open();await hello();say(t('Suche Netzwerke …','Scanning networks …'));
      var found=[];
      await new Promise(function(res,rej){
        var f=function(type,data){if(type===4&&data[0]===CMD_SCAN){var s=strings(data);if(!s.length){done();res()}else found.push(s)}};
        var to=setTimeout(function(){done();rej(new Error('timeout'))},20000);
        function done(){clearTimeout(to);waiters=waiters.filter(function(x){return x!==f})}
        waiters.push(f);send(packet(CMD_SCAN,[])).catch(function(e){done();rej(e)});
      });
      found.sort(function(a,b){return b[1]-a[1]});
      list.innerHTML='';found.forEach(function(s){var o=document.createElement('option');o.value=s[0];list.appendChild(o)});
      say(t(found.length+' Netzwerke gefunden. Klicke in das Feld „WLAN-Name“ für die Liste.',found.length+' networks found. Click the “Wi-Fi name” field for the list.'),'ok');
    }catch(e){say(failText(e),'err')}
    await close();scanBtn.disabled=false;
  };

  form.onsubmit=async function(ev){
    ev.preventDefault();
    var ssid=form.ssid.value.trim(),wpw=form.wifipw.value,apw=form.adminpw.value,apw2=form.adminpw2.value;
    if(apw!==apw2){say(t('Die Admin-Passwörter stimmen nicht überein.','The admin passwords do not match.'),'err');return}
    go.disabled=true;say(t('Verbinde mit der Waage …','Connecting to the scale …'));
    try{
      await open();await hello();
      say(t('Waage verbindet sich mit „'+ssid+'“ … (bis zu 20 s)','Scale is connecting to “'+ssid+'” … (up to 20 s)'));
      var p=wait(function(type,data){
        if(type===2&&data[0]!==0)return {err:data[0]};
        if(type===4&&data[0]===CMD_SETUP){var rs=strings(data);return {url:rs[0],why:rs[1]||''}}
      },40000);
      await send(packet(CMD_SETUP,[ssid,wpw,apw]));
      var r=await p;
      if(r.err){var m=ERR[r.err];say(m?(de()?m[0]:m[1]):t('Unbekannter Fehler','Unknown error'),'err')}
      else if(!r.url){
        // Neuere Firmware: Daten gespeichert, WLAN noch nicht verbunden. Die Waage versucht es weiter.
        form.wifipw.value='';form.adminpw.value='';form.adminpw2.value='';
        var retry=t(' Die Waage versucht es weiter, auch nach einem Neustart, und verbindet sich, sobald es klappt. Das Admin-Passwort ist gesetzt.',' The scale keeps trying, also after a restart, and connects as soon as it can. The admin password is set.');
        var fix=t(' Tippfehler? Einfach hier noch einmal einrichten.',' Typo? Just run the setup here again.');
        if(r.why==='auth')say(t('Gespeichert, aber die Anmeldung bei „'+ssid+'“ ist gescheitert. Meist ist das WLAN-Passwort falsch: bitte hier mit dem richtigen Passwort noch einmal einrichten.','Saved, but signing in to “'+ssid+'” failed. Usually the Wi-Fi password is wrong: please run the setup again with the correct password.')+retry,'err');
        else if(r.why==='notfound')say(t('Gespeichert. Die Waage findet „'+ssid+'“ gerade nicht (zu weit vom Router? nur 2,4 GHz).','Saved. The scale cannot find “'+ssid+'” right now (too far from the router? 2.4 GHz only).')+retry+fix,'ok');
        else say(t('Gespeichert, aber noch nicht mit „'+ssid+'“ verbunden.','Saved, but not yet connected to “'+ssid+'”.')+retry+fix,'ok');
      }
      else{
        form.wifipw.value='';form.adminpw.value='';form.adminpw2.value='';
        st.className='setup-status ok';st.textContent='';
        st.appendChild(document.createTextNode(t('Fertig! Die Waage ist im WLAN. Weboberfläche: ','Done! The scale is on your Wi-Fi. Web UI: ')));
        var a=document.createElement('a');a.href=r.url;a.textContent=r.url;a.target='_blank';a.rel='noopener';st.appendChild(a);
        st.appendChild(document.createTextNode(t(' (Anmeldung: admin + dein neues Passwort)',' (sign in: admin + your new password)')));
      }
    }catch(e){say(e&&e.message==='timeout'?t('Keine Antwort von der Waage.','No answer from the scale.'):failText(e),'err')}
    await close();go.disabled=false;
  };
})();
