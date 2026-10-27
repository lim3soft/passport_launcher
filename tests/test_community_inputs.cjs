/* Optional owner's real input files. No firmware is executed by this test. */
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const tool=require('../main/bin_import.js');
const source=process.argv[2],out=process.argv[3];
if(!source||!fs.existsSync(path.join(source,'input-identities.json'))) {
  console.log('Owner community BIN inputs: NOT RUN (files not present).');process.exit(0);
}
(async()=>{
  const identities=JSON.parse(fs.readFileSync(path.join(source,'input-identities.json'))),results=[];
  for(const r of identities) {
    const raw=fs.readFileSync(path.join(source,r.file));
    assert.equal(crypto.createHash('sha256').update(raw).digest('hex'),r.sha256);
    const start=parseInt(r.app_offset,16);let cursor=start+24,sum=0xef;
    for(let segment=0;segment<raw[start+1];segment++) {
      const n=raw.readUInt32LE(cursor+4);cursor+=8;assert.ok(n<=raw.length-cursor);
      for(let i=0;i<n;i++)sum^=raw[cursor+i];cursor+=n;
    }
    const end=start+Math.floor((cursor-start+16)/16)*16;
    assert.equal(raw[end-1],sum);assert.equal(end+32-start,r.application_bytes);
    assert.deepEqual(crypto.createHash('sha256').update(raw.subarray(start,end)).digest(),raw.subarray(end,end+32));
    let result={...r,esp_image_checksum_and_sha256:'PASS',physical_execution:'NOT RUN'};
    if(r.file==='pokemon.bin') {
      assert.throws(()=>tool.inspect(raw,r.file),/Unsupported merged partition layout/);
      result.import='BLOCKED';result.reason='Custom cardid/save_restore/wifi_config/pokewalk_store/recovery partitions. Its code can fit the shared pool, but the extra data partitions require a source port.';
    } else {
      const info=tool.inspect(raw,r.file);
      assert.equal(info.abi,0);assert.equal(info.image.length,r.application_bytes);
      const name=r.file.startsWith('QSO')?'QSO Vision':'Internet Radio';
      const id=r.file.startsWith('QSO')?'community.qso-vision':'community.internet-radio';
      const payload=Buffer.from(await tool.pack(info,name,id).arrayBuffer());
      assert.equal(payload.readUInt32LE(12),0);assert.equal(payload.length,r.application_bytes+512);
      if(out)fs.writeFileSync(path.join(out,r.file.startsWith('QSO')?'real-qso.transport':'real-radio.transport'),payload);
      result.import='PASS';result.application_sha256=crypto.createHash('sha256').update(info.image).digest('hex');
      result.assigned_id=id;result.idf_compatibility='community ESP-IDF 5.5.x';
    }
    results.push(result);
    console.log(r.file+': '+result.import+'; '+r.application_bytes+' application bytes; '+r.idf_version);
  }
  fs.writeFileSync('docs/community-input-results.json',JSON.stringify({format:'passport-launcher.community-inputs',version:fs.readFileSync('CMakeLists.txt','utf8').match(/set\(PROJECT_VER "([^"]+)"\)/)[1],host_import_test:'PASS',machine_code_executed:false,physical_device_tests:'NOT RUN',files:results},null,2)+'\n');
})().catch(e=>{console.error(e);process.exitCode=1;});
