"""Read x64 PE dependency metadata without mapping or executing a DLL."""
import struct


class PeImage:
    def __init__(self, data):
        self.data = data
        nt = self.u32(0x3c)
        if data[nt:nt + 4] != b'PE\0\0' or self.u16(nt + 4) != 0x8664:
            raise ValueError('Expected an x64 PE image')
        self.optional = nt + 24
        if self.u16(self.optional) != 0x20b:
            raise ValueError('Expected PE32+')
        section_table = self.optional + self.u16(nt + 20)
        self.sections = [struct.unpack_from('<8sIIII', data, section_table + 40 * i)
                         for i in range(self.u16(nt + 6))]

    def u16(self, at):
        return struct.unpack_from('<H', self.data, at)[0]

    def u32(self, at):
        return struct.unpack_from('<I', self.data, at)[0]

    def offset(self, rva):
        if rva < self.u32(self.optional + 60):
            return rva
        for _, virtual_size, address, raw_size, pointer in self.sections:
            if address <= rva < address + min(virtual_size, raw_size):
                return pointer + rva - address
        raise ValueError(f'Unmapped RVA {rva:x}')

    def string(self, rva):
        at = self.offset(rva)
        return self.data[at:self.data.index(0, at)].decode('ascii')

    def exports(self):
        rva = self.u32(self.optional + 112)
        if not rva:
            return {}
        at = self.offset(rva)
        base, count, name_count, addresses, names, ordinals = struct.unpack_from('<6I', self.data, at + 16)
        labels = {self.u16(self.offset(ordinals) + 2 * i): self.string(self.u32(self.offset(names) + 4 * i))
                  for i in range(name_count)}
        return {base + i: labels.get(i) for i in range(count)
                if self.u32(self.offset(addresses) + 4 * i)}

    def imports(self):
        result = {}
        for directory, width in ((1, 20), (13, 32)):
            rva = self.u32(self.optional + 112 + 8 * directory)
            if not rva:
                continue
            at = self.offset(rva)
            while any(self.data[at:at + width]):
                if directory == 1:
                    original, _, _, name, first = struct.unpack_from('<5I', self.data, at)
                else:
                    attributes, name, _, first, original, _, _, _ = struct.unpack_from('<8I', self.data, at)
                    if not attributes & 1:
                        raise ValueError('Unsupported VA-based delay import')
                thunk = self.offset(original or first)
                symbols = result.setdefault(self.string(name).lower(), [])
                while value := struct.unpack_from('<Q', self.data, thunk)[0]:
                    symbols.append(value & 65535 if value >> 63 else self.string(value + 2))
                    thunk += 8
                at += width
        return result


def audit_runtime_dependencies(data):
    imports = PeImage(data).imports()
    if 'd3d12.dll' in imports:
        raise ValueError(f'Eager d3d12.dll import can bind to an app-local shim: {imports["d3d12.dll"]}')
    return imports
