require 'magritte'
require 'stringio'
require 'rspec'

describe 'Decoding Intel Hex files' do
  it 'calculates the correct checksum for records below address 256' do
    result = filter(wrap(
        ':2000000023696E636C756465203C737464696F2E683E0A23696E636C756465203C737464CE'
    ))

    expect(result).not_to include('Error')
  end


  it 'calculates the correct checksum for records in the upper half of the address space' do
    result = filter(wrap(
        ':200100000A3A31303030324630304546463838444630413446464544433546304345413492'
    ))

    expect(result).not_to include('Error')
  end

  it 'errors when there is no end of file' do
    result = filter(
        ':200100000A3A31303030324630304546463838444630413446464544433546304345413492'
    )

    expect(result).to include('end of file without EOF record')
  end

  it 'errors when the checksum does not match' do
    result = filter(wrap(
        ':200100000A3A31303030324630304546463838444630413446464544433546304345413493'
    ))

    expect(result).to include('Mismatched checksum')
  end

  it 'errors when there is a NULL before the end of line' do
    result = filter(wrap(
        ":200100000A3A31303030#{0.chr}4630304546463838444630413446464544433546304345413493"
    ))

    expect(result).to include('NULL before end of line')
  end
end

def wrap(str)
  # Append the EOF record so we don't get early end errors
  "#{str}\n:00000001FF\n"
end

def filter(str)
  buffer = StringIO.new
  Magritte::Pipe.from_input_string(str)
    .out_to(buffer)
    .filtering_with('./main')
  buffer.string()
end
