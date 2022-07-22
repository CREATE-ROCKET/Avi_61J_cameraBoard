import sys
from tqdm import tqdm


def slash(args):
    f = open(args[1], 'r')
    data = f.read().replace('\n', '').split(',')

    header = 0
    header_counter = 0

    data_mpu = ''
    data_lps = ''
    dump = 0

    for i in tqdm(range(len(data))):
        if(header_counter == 0):
            if(data[i] == '224'):
                # mpu
                header = 1
                header_counter = 17
                data_mpu += data[i]
                data_mpu += ','
            elif(data[i] == '227'):
                # lps
                header = 2
                header_counter = 8
                data_lps += data[i]
                data_lps += ','
            else:
                header = 0
                dump += 1
        else:
            if(header == 1):
                # mpu
                data_mpu += data[i]
                if(header_counter == 1):
                    data_mpu += '\n'
                else:
                    data_mpu += ','
            if(header == 2):
                # lps
                data_lps += data[i]
                if(header_counter == 1):
                    data_lps += '\n'
                else:
                    data_lps += ','
        header_counter -= 1

    output_mpu = open('mpu.csv', 'w')
    output_mpu.write(data_mpu)
    output_mpu.close()

    output_lps = open('lps.csv', 'w')
    output_lps.write(data_lps)
    output_lps.close()

    print('dump data count:'+str(dump))


if __name__ == "__main__":
    args = sys.argv
    slash(args)
