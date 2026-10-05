"""Cross-edition circuit, bounds, PQ interoperability, and failure tests."""
import os
from pathlib import Path
import random
import re
import stat
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HEADER = 'OPENQASM 2.0; include "qelib1.inc"; qreg q[2]; '

class QuantumTools(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name)

    def commands(self, name):
        commands = [[sys.executable,str(ROOT/(name+'.py'))]]
        if (ROOT/'bin'/name).exists():
            commands.append([str(ROOT/'bin'/name)])
        elif os.environ.get('PYDEV_REQUIRE_PQ'):
            self.fail('required C binary missing')
        return commands

    def run_tool(self, command, *args, env=None, code=0):
        result = subprocess.run(command + list(map(str,args)), capture_output=True,
                                timeout=15, env=env, cwd=ROOT)
        self.assertEqual(result.returncode,code,result.stderr.decode(errors='replace'))
        return result.stdout.decode()

    def circuit(self, source):
        path = self.path/'circuit.qasm'
        path.write_bytes(source if isinstance(source,bytes) else source.encode())
        return path

    def probabilities(self, command, source):
        output = self.run_tool(command,self.circuit(source))
        return [float(line.split()[1]) for line in output.splitlines() if not line.startswith('#')]

    def test_bell_depth_and_order(self):
        for command in self.commands('qsim'):
            self.assertEqual(self.probabilities(command,HEADER+'h q[0]; cx q[0],q[1];'),[.5,0,0,.5])
            self.assertEqual(self.probabilities(command,HEADER+'x q[0];'),[0,1,0,0])
            self.assertEqual(self.probabilities(command,HEADER+'x q[1]; cx q[1],q[0];'),[0,0,0,1])
        for command in self.commands('qasmcheck'):
            out = self.run_tool(command,self.circuit(HEADER+'h q[0]; h q[1]; cz q[0],q[1];'))
            self.assertIn('gates=3 depth=2',out)

    def test_phase_and_measurement(self):
        for command in self.commands('qsim'):
            for gates in ('h q[0]; z q[0]; h q[0];',
                          'h q[0]; s q[0]; s q[0]; h q[0];',
                          'h q[0]; '+ 't q[0]; '*4+'h q[0];',
                          'x q[1]; h q[0]; cz q[1],q[0]; h q[0]; x q[1];'):
                self.assertEqual(self.probabilities(command,HEADER+gates),[0,1,0,0])
            out = self.probabilities(command,HEADER+'creg c[2]; h q[0]; measure q -> c; // eof')
            self.assertEqual(out,[.5,.5,0,0])

    def test_rejections(self):
        bad = ['', HEADER+'hq[0];', HEADER+'cx q[0],q[0];',HEADER+'h q[2];',
               HEADER+'h q[-1];', HEADER+'h q[0]', HEADER+'reset q[0];',
               HEADER+'h q[0];\0', HEADER+'include "other.inc";',
               HEADER+'measure q -> c;', HEADER+'creg c[1];',
               HEADER+'creg c[2]; measure q -> c; x q[0];',
               HEADER.replace('qelib1.inc',' qelib1.inc'),
               HEADER.replace('qelib1.inc','qelib1.inc//comment\n'),
               HEADER.replace('[2]','[0]'), HEADER.replace('[2]','[21]'),
               HEADER+'x q['+'9'*10000+'];', HEADER+'//'+ 'a'*262144,
               HEADER+'x q[0];'*2049, HEADER+'h q[٠];']
        for tool in ('qasmcheck','qsim'):
            for command in self.commands(tool):
                for source in bad:
                    with self.subTest(command=command,source=source[:100]):
                        self.run_tool(command,self.circuit(source),code=2)

    def test_file_types_and_limits(self):
        source = self.circuit(HEADER+'x q[0];')
        link = self.path/'link'; link.symlink_to(source)
        fifo = self.path/'fifo'; os.mkfifo(fifo)
        for tool in ('qsim','qasmcheck'):
            for command in self.commands(tool):
                for path in (self.path,link,fifo,self.path/'absent'):
                    self.run_tool(command,path,code=2)
        for command in self.commands('qasmcheck'):
            self.assertIn('simulator_supported=no',self.run_tool(command,self.circuit(HEADER.replace('[2]','[20]'))))
            self.run_tool(command,self.circuit(HEADER+'x q[0];'*2048))
        for command in self.commands('qsim'):
            self.run_tool(command,self.circuit(HEADER.replace('[2]','[11]')),code=2)
            p = self.probabilities(command,HEADER.replace('[2]','[10]'))
            self.assertEqual(len(p),1024)
            self.assertEqual(sum(p),1)

    def test_deterministic_fuzz_and_parity(self):
        rng = random.Random(573)
        commands = self.commands('qsim')
        for _ in range(8):
            gates=[]
            for _ in range(35):
                g=rng.choice(['h','x','z','s','t','cx','cz'])
                a=rng.randrange(2)
                gates.append(f'{g} q[{a}]'+(f',q[{1-a}]' if g.startswith('c') else '')+';')
            outputs=[self.probabilities(c,HEADER+''.join(gates)) for c in commands]
            for output in outputs:
                self.assertAlmostEqual(sum(output),1,places=9)
                self.assertTrue(all(0 <= p <= 1 for p in output))
                self.assertEqual(output,outputs[0])
        for command in self.commands('qasmcheck'):
            for _ in range(15):
                data = bytes(rng.randrange(256) for _ in range(200))
                self.run_tool(command,self.circuit(data),code=2)

    def test_budget_and_password(self):
        for command in self.commands('qbudget'):
            self.assertIn('statevector_bytes=17179869184 GiB=16.000000000',self.run_tool(command,'30'))
            self.assertIn('18014398509481984',self.run_tool(command,'50'))
            for bad in ('0','51','-1','nan','1.0','1e2','9'*1000,'+2'):
                self.run_tool(command,bad,code=2)
        for command in self.commands('randpass'):
            values = [self.run_tool(command,'--quantum').strip() for _ in range(3)]
            self.assertEqual(len(set(values)),3)
            self.assertTrue(all(re.fullmatch('[0-9a-f]{64}',s) for s in values))
            self.run_tool(command,'--quantum','64',code=2)

    def test_backend_failures_and_output_bounds(self):
        env = dict(os.environ,PYDEV_OPENSSL='/nonexistent/openssl')
        for command in self.commands('pqcheck'):
            self.run_tool(command,env=env,code=2)
            self.run_tool(command,env=dict(env,PYDEV_OPENSSL='relative'),code=2)
        helper = self.path/'noisy-openssl'
        helper.write_text('#!'+sys.executable+'\nimport os\nos.write(1,b"x"*70000)\n')
        helper.chmod(0o700)
        for command in self.commands('pqcheck'):
            self.run_tool(command,env=dict(env,PYDEV_OPENSSL=str(helper)),code=2)
        for i,command in enumerate(self.commands('pqkey')):
            self.run_tool(command,'RSA',self.path/f'bad{i}',env=env,code=2)
            self.assertFalse((self.path/f'bad{i}').exists())
            self.run_tool(command,'ML-KEM-768',self.path/f'fail{i}',env=env,code=2)
            self.assertFalse((self.path/f'fail{i}'/'private.pem').exists())
            self.run_tool(command,'ML-KEM-768',self.path,env=env,code=2)

    def test_backend_timeout(self):
        helper = self.path/'slow-openssl'
        helper.write_text('#!'+sys.executable+'\nimport time\ntime.sleep(30)\n')
        helper.chmod(0o700)
        for command in self.commands('pqcheck'):
            self.run_tool(command,env=dict(os.environ,PYDEV_OPENSSL=str(helper)),code=2)

    def openssl(self, *args, expected=0):
        exe = os.environ.get('PYDEV_OPENSSL','openssl')
        p = subprocess.run([exe]+list(map(str,args)),capture_output=True,timeout=10)
        self.assertEqual(p.returncode,expected,p.stderr.decode(errors='replace'))
        return p.stdout

    def test_pq_key_interoperability(self):
        exe = os.environ.get('PYDEV_OPENSSL','openssl')
        try:
            p = subprocess.run([exe,'list','-kem-algorithms'],capture_output=True,timeout=10)
            capable = p.returncode == 0 and b'ML-KEM-768' in p.stdout
        except OSError:
            capable = False
        if not capable:
            if os.environ.get('PYDEV_REQUIRE_PQ'):
                self.fail('OpenSSL 3.5+ PQ backend required for this CI job')
            self.skipTest('OpenSSL PQ backend unavailable; run with PYDEV_OPENSSL=...3.5+...')
        message=self.path/'message'; message.write_bytes(b'quantum-control artifact test\n')
        for command in self.commands('pqcheck'):
            out=self.run_tool(command)
            self.assertIn('ML-KEM-768',out); self.assertIn('ML-DSA-65',out)
        for i,command in enumerate(self.commands('pqkey')):
            for algorithm in ('ML-KEM-768','ML-KEM-1024','ML-DSA-65','ML-DSA-87'):
                directory=self.path/f'keys{i}-{algorithm}'
                out=self.run_tool(command,algorithm,directory)
                self.assertNotIn('BEGIN PRIVATE',out)
                private=directory/'private.pem'; public=directory/'public.pem'
                self.assertEqual(stat.S_IMODE(directory.stat().st_mode),0o700)
                for path in (private,public):
                    self.assertEqual(stat.S_IMODE(path.stat().st_mode),0o600)
                original=private.read_bytes()
                self.run_tool(command,algorithm,directory,code=2)
                self.assertEqual(private.read_bytes(),original)
                self.openssl('pkey','-in',private,'-check','-noout')
                if algorithm.startswith('ML-KEM'):
                    ciphertext=directory/'ct'; one=directory/'one'; two=directory/'two'
                    self.openssl('pkeyutl','-encap','-pubin','-inkey',public,'-out',ciphertext,'-secret',one)
                    self.openssl('pkeyutl','-decap','-inkey',private,'-in',ciphertext,'-secret',two)
                    self.assertEqual(len(one.read_bytes()),32)
                    self.assertEqual(one.read_bytes(),two.read_bytes())
                else:
                    signature=directory/'signature'
                    self.openssl('pkeyutl','-sign','-rawin','-inkey',private,'-in',message,'-out',signature)
                    self.openssl('pkeyutl','-verify','-rawin','-pubin','-inkey',public,'-in',message,'-sigfile',signature)
                    corrupted=self.path/'corrupt'; corrupted.write_bytes(b'changed message')
                    self.openssl('pkeyutl','-verify','-rawin','-pubin','-inkey',public,'-in',corrupted,'-sigfile',signature,expected=1)

if __name__ == '__main__':
    unittest.main()
