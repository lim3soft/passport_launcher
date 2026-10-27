import tempfile
import unittest
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import prepare_fresh

class FreshImage(unittest.TestCase):
    def test_complete_clear_and_idempotence(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'fresh.bin';prefix=b'\xe9'+b'A'*0x10000;p.write_bytes(prefix)
            prepare_fresh.prepare(p);out=p.read_bytes()
            self.assertEqual(len(out),0x800000);self.assertEqual(out[:len(prefix)],prefix)
            self.assertEqual(out[len(prefix):],b'\xff'*(len(out)-len(prefix)))
            prepare_fresh.prepare(p);self.assertEqual(p.read_bytes(),out)
    def test_existing_pool_data_or_invalid_input_is_not_overwritten(self):
        for data in [b'wrong',b'\xe9'+b'A'*0x190000,b'\xe9'+b'\xff'*0x800000]:
            with tempfile.TemporaryDirectory() as d:
                p=Path(d)/'image.bin';p.write_bytes(data)
                with self.assertRaises(ValueError):prepare_fresh.prepare(p)
                self.assertEqual(p.read_bytes(),data)
