(function(){
  var btn=document.getElementById('lang'),h=document.documentElement,l='en';
  try{l=localStorage.getItem('lang')||'en'}catch(e){}
  if(l!=='de')l='en';
  function set(x){l=x;h.lang=x;btn.textContent=x==='de'?'EN':'DE';try{localStorage.setItem('lang',x)}catch(e){}}
  btn.onclick=function(){set(l==='de'?'en':'de')};
  set(l);
})();
