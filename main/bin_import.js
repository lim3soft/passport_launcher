/* Local firmware import. No network services, eval, or secure-context-only APIs.
 * Hashes are integrity checks, not publisher authentication. */
(function (root) {
  'use strict';
  const MAX = 0x640000; // Physical shared-pool ceiling; actual capacity is checked by the device.
  const fail = (text,code='invalid',bytes=0) => { const error=new Error(text);error.code=code;error.bytes=bytes;throw error; };
  const equal = (a,b) => a.length === b.length && a.every((v,i) => v === b[i]);
  const u32 = (a,i) => new DataView(a.buffer,a.byteOffset,a.byteLength).getUint32(i,true);
  const rotate = (v,n) => (v >>> n) | (v << (32-n));
  function sha256(bytes) {
    const K = [0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
    const data = new Uint8Array(Math.ceil((bytes.length+9)/64)*64);
    data.set(bytes);data[bytes.length]=128;
    const dv = new DataView(data.buffer);dv.setUint32(data.length-4,bytes.length*8,false);
    const h = new Uint32Array([0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19]);
    const w = new Uint32Array(64);
    for(let offset=0;offset<data.length;offset+=64) {
      for(let i=0;i<16;i++)w[i]=dv.getUint32(offset+i*4,false);
      for(let i=16;i<64;i++) {
        let x=w[i-15],y=w[i-2];
        w[i]=w[i-16]+(rotate(x,7)^rotate(x,18)^(x>>>3))+w[i-7]+(rotate(y,17)^rotate(y,19)^(y>>>10));
      }
      let [a,b,c,d,e,f,g,j]=h;
      for(let i=0;i<64;i++) {
        const t1=(j+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+K[i]+w[i])>>>0;
        const t2=((rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c)))>>>0;
        j=g;g=f;f=e;e=(d+t1)>>>0;d=c;c=b;b=a;a=(t1+t2)>>>0;
      }
      [a,b,c,d,e,f,g,j].forEach((v,i)=>{h[i]=(h[i]+v)>>>0;});
    }
    const out=new Uint8Array(32),v=new DataView(out.buffer);
    h.forEach((x,i)=>v.setUint32(i*4,x,false));return out;
  }
  function md5(bytes) {
    const data=new Uint8Array(Math.ceil((bytes.length+9)/64)*64);data.set(bytes);data[bytes.length]=128;
    new DataView(data.buffer).setUint32(data.length-8,bytes.length*8,true);
    const h=new Uint32Array([0x67452301,0xefcdab89,0x98badcfe,0x10325476]);
    const shifts=[7,12,17,22,5,9,14,20,4,11,16,23,6,10,15,21];
    for(let offset=0;offset<data.length;offset+=64) {
      let [a,b,c,d]=h;
      for(let i=0;i<64;i++) {
        let f,g;
        if(i<16){f=(b&c)|(~b&d);g=i;}
        else if(i<32){f=(d&b)|(~d&c);g=(5*i+1)%16;}
        else if(i<48){f=b^c^d;g=(3*i+5)%16;}
        else{f=c^(b|~d);g=(7*i)%16;}
        let x=(a+f+Math.floor(Math.abs(Math.sin(i+1))*4294967296)+u32(data,offset+g*4))>>>0;
        const n=shifts[Math.floor(i/16)*4+i%4];
        a=d;d=c;c=b;b=(b+((x<<n)|(x>>>(32-n))))>>>0;
      }
      [a,b,c,d].forEach((v,i)=>{h[i]=(h[i]+v)>>>0;});
    }
    const out=new Uint8Array(16),v=new DataView(out.buffer);h.forEach((x,i)=>v.setUint32(i*4,x,true));return out;
  }
  function text(bytes,start,size) {
    const field=bytes.subarray(start,start+size),end=field.indexOf(0);
    if(end<0)fail('Unterminated firmware text field.');
    const raw=field.subarray(0,end);
    if(raw.some(c=>c<32||c>126))fail('Firmware metadata must use printable ASCII.');
    return String.fromCharCode(...raw);
  }
  function imageEnd(bytes,start,limit) {
    if(limit-start<288 || bytes[start]!==0xe9 || bytes[start+1]<1 || bytes[start+1]>16)fail('Not a supported ESP application image.');
    if(bytes[start+2]!==2||bytes[start+3]!==0x3f||bytes[start+12]!==5||bytes[start+13]!==0||bytes[start+23]!==1)
      fail('Requires ESP32-C3, DIO, 80 MHz, 8 MB and an appended image hash.');
    if(u32(bytes,start+32)!==0xabcd5432)fail('Missing ESP-IDF application descriptor.');
    const idf=text(bytes,start+144,32);
    if(!/^v5\.5(?:\.\d+)?(?:-.*)?$/.test(idf))fail('Community import supports ESP-IDF 5.5.x firmware.');
    let cursor=start+24,sum=0xef;
    for(let i=0;i<bytes[start+1];i++) {
      if(cursor+8>limit)fail('Truncated image segment header.');
      const n=u32(bytes,cursor+4);cursor+=8;
      if((i===0&&n<256)||n>limit-cursor)fail('Invalid or truncated image segment.');
      for(let j=cursor;j<cursor+n;j++)sum^=bytes[j];cursor+=n;
    }
    const end=start+Math.floor((cursor-start+16)/16)*16;
    if(end+32>limit || bytes[end-1]!==sum)fail('Firmware checksum failed.');
    if(!equal(sha256(bytes.subarray(start,end)),bytes.subarray(end,end+32)))fail('Firmware image hash failed.');
    return end+32;
  }
  function baselineTable(bytes) {
    const expected=[['nvs',1,2,0x9000,0x6000],['phy_init',1,1,0xf000,0x1000],['factory',0,0,0x10000,0x7f0000]];
    for(let i=0;i<3;i++) {
      const o=0x8000+i*32,[name,type,sub,offset,size]=expected[i];
      if(bytes[o]!==0xaa||bytes[o+1]!==0x50||bytes[o+2]!==type||bytes[o+3]!==sub||
         u32(bytes,o+4)!==offset||u32(bytes,o+8)!==size||text(bytes,o+12,16)!==name||u32(bytes,o+28)!==0)
        fail('Unsupported merged partition layout. Extra resources/data/OTA partitions need a port; upload was not started.','layout');
    }
    const marker=0x8060;
    if(bytes[marker]!==0xeb||bytes[marker+1]!==0xeb||!equal(md5(bytes.subarray(0x8000,marker)),bytes.subarray(marker+16,marker+32)))
      fail('Partition table integrity check failed.');
    if(bytes.subarray(marker+32,0x9000).some(x=>x!==255))fail('Unexpected extra partition-table data.');
    if(bytes.subarray(0x9000,0x10000).some(x=>x!==255))
      fail('Merged file includes initial NVS/PHY data; this importer requires blank data regions.','layout');
  }
  function sdkId(image) {
    const prefix='PASSPORTOS_ABI2:';
    const old=new TextEncoder().encode('PASSPORTOS_ABI1:');
    for(let i=0;i<=image.length-old.length;i++)if(image[i]===80&&old.every((v,j)=>image[i+j]===v))
      fail('This native app targets fixed-slot SDK 1. Rebuild it with the shared-storage SDK 2, or use the original community BIN.');
    const ids=new Set();
    for(let i=0;i<=image.length-prefix.length;i++) {
      if(image[i]!==80)continue;
      let match=true;for(let j=0;j<prefix.length;j++)if(image[i+j]!==prefix.charCodeAt(j)){match=false;break;}
      if(!match)continue;
      let end=i+prefix.length;while(end<image.length&&image[end]!==0&&end-i-prefix.length<64)end++;
      const id=String.fromCharCode(...image.subarray(i+prefix.length,end));
      if(image[end]!==0||!/^[a-z][a-z0-9._-]{0,62}$/.test(id))fail('Invalid launcher SDK declaration.');
      ids.add(id);
    }
    if(ids.size>1)fail('Conflicting launcher SDK app IDs.');
    return [...ids][0]||null;
  }
  function inspect(input,filename) {
    const bytes=input instanceof Uint8Array?input:new Uint8Array(input);
    if(bytes.length>0x800000)fail('File exceeds the board\'s 8 MB flash capacity.');
    let start=0,merged=false;
    if(bytes.length>=0x10120 && bytes[0x8000]===0xaa&&bytes[0x8001]===0x50) {
      if(bytes[0]!==0xe9||bytes[12]!==5||bytes[13]!==0)fail('Merged bootloader is not ESP32-C3.');
      baselineTable(bytes);start=0x10000;merged=true;
    }
    const end=imageEnd(bytes,start,bytes.length),image=bytes.slice(start,end);
    if(image.length>MAX)fail('Application cannot fit alongside the launcher and its private settings.','too_large',image.length);
    if(bytes.subarray(end).some(x=>x!==255))fail('Unexpected trailing data/resources or signed image; requires a compatible port.');
    const id=sdkId(image),project=text(image,80,32),version=text(image,48,32);
    const idf=text(image,144,32);
    if(id&&idf!=='v5.5.3'&&!idf.startsWith('v5.5.3-'))fail('Launcher SDK apps require ESP-IDF 5.5.3.');
    const name=(filename||project||'Community app').replace(/\.bin$/i,'').slice(0,31);
    const stem=name.replace(/[-_ ](?:latest)?v?\d+(?:\.\d+){1,3}.*$/i,'').replace(/[-_ ]latest.*$/i,'');
    const slug=stem.toLowerCase().replace(/[^a-z0-9._-]+/g,'-').replace(/^[^a-z]+/,'').slice(0,53)||'app';
    return {image,merged,project,idf,version:version||'community',name,id:id||'community.'+slug,abi:id?2:0};
  }
  function pack(info,name,id) {
    if(info.abi===2&&id!==info.id)fail('Keep the native SDK app ID unchanged.');
    if(!/^[a-z][a-z0-9._-]{0,62}$/.test(id))fail('App ID: start with a lowercase letter; use letters, digits, dots, - or _. Maximum 63 characters.');
    const h=new Uint8Array(512),view=new DataView(h.buffer);
    const put=(value,offset,width)=>{
      if(!value||value.length>=width||[...value].some(c=>c.charCodeAt(0)<32||c.charCodeAt(0)>126))fail('App name/version must be printable ASCII and at most 31 characters.');
      for(let i=0;i<value.length;i++)h[offset+i]=value.charCodeAt(i);
    };
    put('PPTOS01',0,8);view.setUint32(8,1,true);view.setUint32(12,info.abi,true);view.setUint32(16,info.image.length,true);
    put(id,20,64);put(name,84,32);put(info.version.slice(0,31),116,32);h.set(sha256(info.image),148);
    return new Blob([h,info.image]);
  }
  const api={inspect,pack,sha256,md5};
  if(typeof module!=='undefined'&&module.exports)module.exports=api;
  root.PassportBin=api;
})(typeof globalThis!=='undefined'?globalThis:this);
