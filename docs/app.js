(function(){
  var btn=document.getElementById('lang'),h=document.documentElement,l='en';
  try{l=localStorage.getItem('lang')||'en'}catch(e){}
  if(l!=='de')l='en';
  function set(x){l=x;h.lang=x;btn.textContent=x==='de'?'EN':'DE';try{localStorage.setItem('lang',x)}catch(e){}}
  btn.onclick=function(){set(l==='de'?'en':'de')};
  set(l);
})();

(function(){
  var d=document.documentElement;
  if(!('IntersectionObserver' in window)||matchMedia('(prefers-reduced-motion:reduce)').matches)return;
  d.classList.add('js');
  var els=document.querySelectorAll('.hero-text > *,.hero-img,section > *,.steps li,.features > div,.page > h2,.page > h3');
  var io=new IntersectionObserver(function(es){es.forEach(function(e){if(e.isIntersecting){e.target.classList.add('in');io.unobserve(e.target)}})},{rootMargin:'0px 0px -8% 0px'});
  els.forEach(function(el,i){el.classList.add('rv');el.style.setProperty('--d',(el.matches('.steps li,.features > div')?(i%4)*0.08:0)+'s');io.observe(el)});
})();
