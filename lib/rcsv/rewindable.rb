require 'stringio'

class Rewindable
  def initialize(source)
    @source = source
    @rewound = false
    @replay_buffer = StringIO.new
  end

  def read(n = nil)
    if @rewound
      data = @replay_buffer.read(n)
      if data.nil?
        data = @source.read(n)
      elsif n.nil?
        data += @source.read
      elsif data.size < n
        data += @source.read(n - data.size) || ''
      end
    else
      data = @source.read(n)
      @replay_buffer.write(data)
    end
    data
  end

  def first_line(sep = $/)
    block = ''
    loop do
      data = read(1000)
      break if data.nil? # Fishy
      sep_pos = data.index(sep)
      block += data[0..(sep_pos || 1000)]
      break if sep_pos
    end
    rewind
    nil if block.empty?
    block
  end

  def rewind
    raise IOError.new('Rewindable already rewound!') if @rewound
    @rewound = true
    @replay_buffer.rewind
  end

  def close
    @source.close
  end
end
