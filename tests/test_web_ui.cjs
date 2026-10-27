const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const source = fs.readFileSync('main/web.c', 'utf8');
function cString(name) {
  const body = source.split(`static const char ${name}[] =`)[1].split(';\n')[0];
  return [...body.matchAll(/"(?:\\.|[^"\\])*"/g)].map(m => JSON.parse(m[0])).join('');
}
const html = cString('html') + '0'.repeat(32) + cString('script');
const code = html.split('<script>')[1].split('</script>')[0];
function element(tag) {
  return {tag, children: [], textContent: '', files: [],
    append(...children) {this.children.push(...children);},
    replaceChildren(...children) {this.children = children;},
    removeAttribute(name){delete this[name];}};
}
const elements = Object.fromEntries(['status','apps','file','install','identity','appname','appid','capacity','install-progress','progress-note','transfer'].map(id => ['#'+id, element(id)]));
const requests = [],xhrs=[];
class MockXHR {
  constructor(){this.upload={};this.headers={};xhrs.push(this);}
  open(method,path){this.method=method;this.path=path;}
  setRequestHeader(name,value){this.headers[name]=value;}
  send(body){this.body=body;requests.push({path:this.path,options:{method:this.method,headers:this.headers,body}});}
}
let accepted = true;
const confirmations=[];
const metadata={kind:'512-byte metadata only'};
const binBody={kind:'prepared BIN transport',slice:(start,end)=>{assert.equal(start,0);assert.equal(end,512);return metadata;}};
let preflightOK=true;
let apps = [{slot:1,id:'org.passportos.qso-vision',name:'<img src=x>',version:'test',state:'ready'},
            {slot:2,id:'org.passportos.counter-a',name:'Counter',version:'test',state:'ready'}];
const context = {
  XMLHttpRequest:MockXHR,
  PassportBin: {
    inspect: buffer=>{if(new Uint8Array(buffer)[0]!==0xe9)throw new Error('Invalid firmware');return {name:'Radio',id:'community.radio',abi:0,merged:false,image:new Uint8Array(buffer)};},
    pack: (info,name,id)=>{assert.equal(name,'Radio');assert.equal(id,'community.radio');return binBody;}
  },
  document: {querySelector: id => elements[id], createElement: element},
  confirm: message => {confirmations.push(message);return accepted;},
  fetch: async (path, options) => {
    requests.push({path, options});
    return {ok:path==='/api/preflight'?preflightOK:true,
      json:async () => path==='/api/capacity'?{total_bytes:6684672,free_bytes:4194304,largest_image_bytes:4000000,used_image_bytes:2000000,allocated_bytes:2490368}:apps,
      text:async () => path==='/api/preflight'?'No app space available. Uninstall an app first.':'Done'};
  }
};
const tick = () => new Promise(resolve => setImmediate(resolve));
(async () => {
  vm.runInNewContext(code, context);
  await tick();
  assert.equal(elements['#apps'].children.length, 2);
  const row = elements['#apps'].children[0];
  assert.equal(row.children[0].textContent, '<img src=x>');
  const input = row.children.find(x => x.tag === 'input');
  const restore = row.children.find(x => x.textContent === 'Import legacy settings');
  assert.ok(input && restore);
  assert.equal(elements['#apps'].children[1].children.some(x => x.tag === 'input'), false);
  await restore.onclick();
  assert.equal(requests.filter(x => x.options).length, 0);
  const file = {name:'synthetic.qso-settings'};
  input.files = [file]; accepted = false;
  await restore.onclick();
  assert.equal(requests.filter(x => x.options).length, 0);
  accepted = true;
  await restore.onclick();
  const request = requests.find(x => x.options);
  assert.equal(request.path, '/api/qso-import?slot=1');
  assert.equal(request.options.method, 'POST');
  assert.equal(request.options.headers['X-Passport-Token'], '0'.repeat(32));
  assert.equal(request.options.body, file);
  apps = [{...apps[0],state:'incomplete - retry install'}];
  await vm.runInNewContext('list()', context);
  assert.equal(elements['#apps'].children[0].children.some(x => x.tag === 'input'), false);
  const uninstall=row.children.find(x=>x.textContent==='Uninstall');assert.ok(uninstall);
  accepted=false;await uninstall.onclick();await tick();
  assert.equal(requests.filter(x=>x.path.startsWith('/api/uninstall')).length,0);
  accepted=true;await uninstall.onclick();await tick();
  const removal=requests.find(x=>x.path==='/api/uninstall?slot=1');
  assert.equal(removal.options.headers['X-Passport-App-ID'],'org.passportos.qso-vision');
  assert.match(confirmations.at(-1),/All saved data and settings will be permanently erased/);
  assert.equal(row.children.some(x=>/keep settings|Delete app and settings/.test(x.textContent)),false);
  for(const name of ['test.passport','test.passportos','wrong.txt']) {
    elements['#file'].files=[{name}];await elements['#file'].onchange();await elements['#install'].onclick();
  }
  assert.equal(requests.filter(x=>x.path.startsWith('/api/install')).length,0);
  const binFile={name:'radio.BIN',arrayBuffer:async()=>new Uint8Array([0xe9,0]).buffer};
  elements['#file'].files=[binFile];await elements['#file'].onchange();
  let installation=elements['#install'].onclick();await tick();let upload=xhrs.at(-1);
  assert.equal(elements['#install'].disabled,true);assert.equal(elements['#file'].disabled,true);
  assert.equal(upload.headers['X-Passport-Token'],'0'.repeat(32));
  upload.upload.onprogress({lengthComputable:true,loaded:42,total:100});
  assert.equal(elements['#install-progress'].value,42);assert.match(elements['#progress-note'].textContent,/42%/);
  await elements['#install'].onclick();assert.equal(xhrs.length,1); // duplicate clicks cannot queue another install
  upload.upload.onload();assert.equal(elements['#install-progress'].value,undefined);
  assert.match(elements['#progress-note'].textContent,/Installing and verifying/);
  assert.equal(elements['#install'].disabled,true);
  upload.status=200;upload.responseText='Installed';upload.onload();await installation;
  assert.equal(elements['#install-progress'].value,100);assert.match(elements['#progress-note'].textContent,/Installation complete/);
  assert.equal(elements['#install'].disabled,false);assert.equal(elements['#file'].disabled,false);
  assert.equal(requests.find(x=>x.path==='/api/install-bin').options.body,binBody);
  assert.equal(requests.find(x=>x.path==='/api/preflight').options.body,metadata);
  assert.ok(requests.findIndex(x=>x.path==='/api/preflight')<requests.findIndex(x=>x.path==='/api/install-bin'));
  assert.match(elements['#capacity'].textContent,/4.00 MiB free of 6.38 MiB/);
  const count=requests.filter(x=>x.path==='/api/install-bin').length;
  preflightOK=false;await elements['#install'].onclick();
  assert.equal(requests.filter(x=>x.path==='/api/install-bin').length,count);
  assert.match(elements['#status'].textContent,/No app space available/);preflightOK=true;
  elements['#file'].files=[{name:'wrong.bin',arrayBuffer:async()=>new ArrayBuffer(10)}];
  await elements['#file'].onchange();await elements['#install'].onclick();
  assert.equal(requests.filter(x=>x.path==='/api/install-bin').length,count);
  assert.match(elements['#status'].textContent,/valid .bin/);
  assert.ok(requests.some(x=>x.path==='/api/import-error'&&JSON.parse(x.options.body).code==='invalid'));
  // Upload completion alone is never reported as installation completion.
  // Server rejection and network/timeout failures leave a clear retry state.
  for(const failure of ['http','network','timeout']) {
    elements['#file'].files=[binFile];await elements['#file'].onchange();
    installation=elements['#install'].onclick();await tick();upload=xhrs.at(-1);
    upload.upload.onprogress({lengthComputable:false});assert.equal(elements['#install-progress'].value,undefined);
    upload.upload.onload();assert.doesNotMatch(elements['#progress-note'].textContent,/complete/);
    if(failure==='http'){upload.status=400;upload.responseText='Image validation failed';upload.onload();}
    else if(failure==='network')upload.onerror();else upload.ontimeout();
    await installation;assert.equal(elements['#install-progress'].value,0);assert.equal(elements['#install'].disabled,false);
    assert.doesNotMatch(elements['#progress-note'].textContent,/Installation complete/);
    assert.match(elements['#status'].textContent,failure==='http'?/validation failed/:/not confirmed/);
  }
  assert.match(html,/<progress/);assert.match(html,/Developed by 9W3SPY/);
  assert.match(html,/href=https:\/\/github.com\/lim3soft\/passport_launcher[^>]*>GitHub<\/a>/);
  assert.doesNotMatch(html,/For an installed QSO Vision app|3 app positions|2 MiB of application code/);
  assert.match(cString('about_tail'),/https:\/\/github.com\/lim3soft\/passport_launcher/);
  assert.match(html,/accept='.bin'/);assert.doesNotMatch(html,/\.passport(?:os)?(?:['"\s<>])/);assert.match(html,/<title>Passport Launcher/);
  assert.match(html,/href=\/about/);
  assert.doesNotMatch(source,/community-download|community_helper/);
  assert.match(cString('about_html'),/Developed by/);assert.match(cString('about_html'),/9W3SPY/);
  assert.match(cString('about_tail'),/MIT/);assert.match(cString('about_tail'),/ESP-IDF 5.5.3/);
  assert.match(cString('about_tail'),/M5 Launcher/);assert.match(cString('about_tail'),/\/licenses.txt/);
  console.log('Embedded browser UI: BIN-only selection, token, confirmed identity-bound uninstall, cancel, real upload progress/server completion/errors, GitHub/footer, About/license links and safe text: PASS');
})().catch(error => {console.error(error);process.exitCode=1;});
