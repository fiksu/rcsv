require 'test/unit'
require 'rcsv'

class RcsvParseTest < Test::Unit::TestCase
  def setup
    @options = {
      :header => true,
      :columns => {
        'Date' => {
          :type => :string
        }
      }
    }
  end

  def test_rcsv_parse_unknown_rows
    csv = "a,b,c,d,e\n1,2,3,4,5"
    parsed_data = Rcsv.parse(csv,
      :row_as_hash => true,
      :columns => {
        'b' => {
          :type => :int,
          :alias => 'B'
        },
        'd' => {
          :type => :int
        }
      }
    )

    assert_equal({
      'a' => '1',
      'B' => 2,
      'c' => '3',
      'd' => 4,
      'e' => '5'
    }, parsed_data.first)
  end

  def test_rcsv_parse_only_rows
    csv = "a,1,t\nb,2,false\nc,3,0"
    parsed_data = Rcsv.parse(csv,
      :header => :none,
      :columns => [
        {
          :match => ['z', 'a', '1']
        },
        {},
        {
          :match => true,
          :type => :bool
        }
      ]
    )

    assert_equal([["a", "1", true]], parsed_data)
  end

  def test_rcsv_parse_only_rows_not_match
    csv = "a,1,t\nb,2,false\nc,3,0"
    parsed_data = Rcsv.parse(csv,
      :header => :none,
      :columns => [
        {
          :not_match => ['z', 'a', '1']
        },
        {
          :type => :int
        },
        {
          :not_match => true,
          :type => :bool
        }
      ]
    )

    assert_equal([["b", 2, false], ["c", 3, false]], parsed_data)
  end

  if String.instance_methods.include?(:encoding)
    def test_rcsv_parse_encoding
      utf8_csv = "a,b,c".force_encoding("UTF-8")
      ascii_csv = "a,b,c".force_encoding("ASCII-8BIT")
      
      parsed_utf8_data = Rcsv.parse(utf8_csv, :header => :none)
      parsed_ascii_data = Rcsv.parse(ascii_csv, :header => :none)
      
      assert_equal(parsed_utf8_data.first.first.encoding, Encoding::UTF_8)
      assert_equal(parsed_ascii_data.first.first.encoding, Encoding::ASCII_8BIT)
    end
  end
end
