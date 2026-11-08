(function(){
  var btn=document.getElementById('lang'),h=document.documentElement,l='de';
  try{l=localStorage.getItem('lang')||((navigator.language||'de').slice(0,2)==='de'?'de':'en')}catch(e){}
  function set(x){l=x;h.lang=x;btn.textContent=x==='de'?'EN':'DE';try{localStorage.setItem('lang',x)}catch(e){}}
  btn.onclick=function(){set(l==='de'?'en':'de')};
  set(l);
})();
