"""Read-only GAFE01 ISO/CISO access for local development tools."""
import struct

class Disc:
    def __init__(self,path):
        self.file=open(path,'rb')
        h=self.file.read(0x8000)
        self.block_size=struct.unpack_from('<I',h,4)[0] if h[:4]==b'CISO' else 0
        self.blocks=[];count=0
        if self.block_size:
            for flag in h[8:]:
                self.blocks.append(count if flag else -1)
                if flag:count+=1
        boot=self.read(0,0x440)
        if boot[:6]!=b'GAFE01' or boot[7]!=0:raise ValueError('Unsupported disc revision')
        offset,size=struct.unpack_from('>II',boot,0x424)
        fst=self.read(offset,size);entries=struct.unpack_from('>I',fst,8)[0]
        self.files={}
        for i in range(1,entries):
            word,offset,size=struct.unpack_from('>III',fst,i*12)
            if word>>24:continue
            start=entries*12+(word&0xffffff)
            name=fst[start:fst.index(b'\0',start)].decode('ascii')
            self.files[name]=(offset,size)
    def read(self,offset,size):
        if not self.block_size:
            self.file.seek(offset);data=self.file.read(size)
            if len(data)!=size:raise ValueError('Truncated image')
            return data
        out=bytearray()
        while size:
            block,inner=divmod(offset,self.block_size)
            n=min(size,self.block_size-inner);physical=self.blocks[block]
            if physical<0:out.extend(bytes(n))
            else:
                self.file.seek(0x8000+physical*self.block_size+inner)
                data=self.file.read(n)
                if len(data)!=n:raise ValueError('Truncated CISO')
                out.extend(data)
            offset+=n;size-=n
        return bytes(out)

    def archive_file(self,archive,name):
        blob=self.read(*self.files[archive])
        if blob[:4]!=b'RARC':raise ValueError('Expected uncompressed RARC')
        data_base=0x20+struct.unpack_from('>I',blob,0xc)[0]
        count,entries=struct.unpack_from('>II',blob,0x28)
        strings=0x20+struct.unpack_from('>I',blob,0x34)[0]
        for i in range(count):
            p=0x20+entries+i*20
            flags=struct.unpack_from('>I',blob,p+4)[0]
            start=strings+(flags&0xffffff)
            entry=blob[start:blob.index(b'\0',start)].decode('ascii')
            if entry==name and flags>>24&1:
                offset,size=struct.unpack_from('>II',blob,p+8)
                result=blob[data_base+offset:data_base+offset+size]
                if result[:4] in (b'Yaz0',b'Yay0'):raise ValueError('Compressed entry unsupported')
                return result
        raise ValueError('Missing archive entry '+name)
