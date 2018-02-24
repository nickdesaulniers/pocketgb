from collections import OrderedDict


def parse_template(template_fname):
    with open('templates/' + template_fname, 'rb') as f:
        items = f.read().replace('\n', '').replace(' ', '').split(',')
        assert len(items) == 256
        return items


def get_instruction_list():
    return zip(
        parse_template('opcodes.txt'), parse_template('operand_0.txt'),
        parse_template('operand_1.txt'))


def operand_len(operand):
    keys = operand_decode_table.keys()
    try:
        index = keys.index(operand)
    except ValueError:
        return 0
    if index >= keys.index('kd16'):
        return 2
    if index >= keys.index('kd8'):
        return 1
    return 0


def instruction_len(instruction):
    # TODO: handle CB and oddities like STOP
    return 1 + operand_len(instruction[1]) + operand_len(instruction[2])


def decode_operand(enum):
    if not enum in operand_decode_table:
        raise ValueError('Undecodeable operand: ' + enum)
    return operand_decode_table[enum]


def decode_instruction(instruction):
    assert len(instruction) == 3
    print(instruction)
    op = instruction[0]
    if op == 'kNOP':
        pass
    elif op == 'kLD':
        handle_ld(instruction[1], instruction[2])
    elif op == 'kINC':
        handle_inc(instruction[1])
    else:
        raise ValueError('Unhandleable opcode: ' + op)
    print('  lr35902->registers.pc += %d;' % instruction_len(instruction))
    print('  break;')


# Specefic instructions
def handle_ld(dst, src):
    if dst.startswith('kDEREF'):
        print('  ' + (wb(decode_operand(dst), decode_operand(src))))
    else:
        print('  %s = %s;' % (decode_operand(dst), decode_operand(src)))


def handle_inc(dst):
    if dst == 'kDEREF_HL':
        raise ValueError('Unhandled LD to (HL)')
    # TODO: handle flags
    print('  ++%s;' % decode_operand(dst))


def reg(reg):
    return 'lr35902->registers.' + reg

def rb(addr):
    return 'rb(lr35902->mmu, %s)' % addr

def wb(addr, val):
    return 'wb(lr35902->mmu, %s, %s)' % (addr, val)

operand_decode_table = OrderedDict([
    ('kA', reg('a')),
    # ('kB', ''),
    # ('kC', ''),
    # ('kDEREF_C', ''),
    # ('kD', ''),
    # ('kE', ''),
    # ('kH', ''),
    # ('kL', ''),
    # ('kAF', ''),
    ('kBC', reg('bc')),
    ('kDEREF_BC', reg('bc')),
    # ('kDE', ''),
    # ('kDEREF_DE', ''),
    # ('kHL', ''),
    # ('kDEREF_HL', ''),
    # ('kDEREF_HL_INC', ''),
    # ('kDEREF_HL_DEC', ''),
    # ('kSP', ''),
    # ('kCOND_Z', ''),
    # ('kCOND_NZ', ''),
    # ('kCOND_C', ''),
    # ('kCOND_NC', ''),
    # ('kBIT_0', ''),
    # ('kBIT_1', ''),
    # ('kBIT_2', ''),
    # ('kBIT_3', ''),
    # ('kBIT_4', ''),
    # ('kBIT_5', ''),
    # ('kBIT_6', ''),
    # ('kBIT_7', ''),
    # ('kLITERAL_0x00', ''),
    # ('kLITERAL_0x08', ''),
    # ('kLITERAL_0x10', ''),
    # ('kLITERAL_0x18', ''),
    # ('kLITERAL_0x20', ''),
    # ('kLITERAL_0x28', ''),
    # ('kLITERAL_0x30', ''),
    # ('kLITERAL_0x38', ''),
    ('kd8', 'load_d8(lr35902)'),
    # ('kr8', ''),
    # ('kSP_r8', ''),
    # ('kDEREF_a8', ''),
    ('kd16', 'load_d16(lr35902)'),
    # ('ka16', ''),
    # ('kDEREF_a16', ''),
])

for i, instruction in enumerate(get_instruction_list()):
    print('case 0x%s:' % format(i, '02X'))
    decode_instruction(instruction)
    if i == 2:
        break
