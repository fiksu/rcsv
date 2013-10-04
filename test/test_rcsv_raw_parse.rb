require 'test/unit'
require 'rcsv'

class RcsvRawParseTest < Test::Unit::TestCase
  def setup
    @csv_data = File.open('test/test_rcsv.csv')
  end

  def test_rcsv
    raw_parsed_csv_data = Rcsv.raw_parse(@csv_data)

    assert_equal('EDADEDADEDADEDADEDADEDAD', raw_parsed_csv_data[0][2])
    assert_equal('$$$908080', raw_parsed_csv_data[0][13])
    assert_equal('"', raw_parsed_csv_data[0][14])
    assert_equal('true/false', raw_parsed_csv_data[0][15])
    assert_equal(nil, raw_parsed_csv_data[0][16])
    assert_equal(nil, raw_parsed_csv_data[9][2])
    assert_equal('""C81E-=; **ECCB; .. 89', raw_parsed_csv_data[3][6])
    assert_equal('Dallas, TX', raw_parsed_csv_data[888][13])
  end

  def test_rcsv_col_sep
    tsv_data = StringIO.new(@csv_data.read.tr(",", "\t"))
    raw_parsed_tsv_data = Rcsv.raw_parse(tsv_data, :col_sep => "\t")

    assert_equal('EDADEDADEDADEDADEDADEDAD', raw_parsed_tsv_data[0][2])
    assert_equal('$$$908080', raw_parsed_tsv_data[0][13])
    assert_equal('"', raw_parsed_tsv_data[0][14])
    assert_equal('true/false', raw_parsed_tsv_data[0][15])
    assert_equal(nil, raw_parsed_tsv_data[0][16])
    assert_equal(nil, raw_parsed_tsv_data[9][2])
    assert_equal('""C81E-=; **ECCB; .. 89', raw_parsed_tsv_data[3][6])
    assert_equal("Dallas\t TX", raw_parsed_tsv_data[888][13])
  end
  
  if String.instance_methods.include?(:encoding)
    def test_rcsv_output_encoding_default
      raw_parsed_csv_data = Rcsv.raw_parse(@csv_data)
    
      assert_equal(raw_parsed_csv_data[0][2].encoding, Encoding::ASCII_8BIT)
    end

    def test_rcsv_output_encoding_utf8
      raw_parsed_csv_data = Rcsv.raw_parse(@csv_data, :output_encoding => "UTF-8")
      
      assert_equal(raw_parsed_csv_data[0][2].encoding, Encoding::UTF_8)
    end
  else
    def test_rcsv_encoding_unavailable
      assert_raise Rcsv::ParseError do
        Rcsv.raw_parse(@csv_data, :output_encoding => "UTF-8")
      end
    end
  end
    
  def test_buffer_size
    raw_parsed_csv_data = Rcsv.raw_parse(@csv_data, :buffer_size => 10)

    assert_equal('EDADEDADEDADEDADEDADEDAD', raw_parsed_csv_data[0][2])
    assert_equal('$$$908080', raw_parsed_csv_data[0][13])
    assert_equal('"', raw_parsed_csv_data[0][14])
    assert_equal('true/false', raw_parsed_csv_data[0][15])
    assert_equal(nil, raw_parsed_csv_data[0][16])
    assert_equal(nil, raw_parsed_csv_data[9][2])
    assert_equal('""C81E-=; **ECCB; .. 89', raw_parsed_csv_data[3][6])
    assert_equal('Dallas, TX', raw_parsed_csv_data[888][13])
  end

  def test_single_item_csv
    raw_parsed_csv_data = Rcsv.raw_parse(StringIO.new("Foo"))

    assert_equal([["Foo"]], raw_parsed_csv_data)
  end

  def test_broken_data
    broken_data = StringIO.new(@csv_data.read.sub(/"/, ''))

    assert_raise(Rcsv::ParseError) do
      Rcsv.raw_parse(broken_data)
    end
  end

  def test_broken_data_without_strict
    broken_data = StringIO.new(@csv_data.read.sub(/"/, ''))

    raw_parsed_csv_data = Rcsv.raw_parse(broken_data, :nostrict => true)
    assert_equal(["DSAdsfksjh", "iii ooo iii", "EDADEDADEDADEDADEDADEDAD", "111 333 555", "NMLKTF", "---==---", "//", "###", "0000000000", "Asdad bvd qwert", ";'''sd", "@@@", "OCTZ", "$$$908080", "\",true/false\nC85A5B9F,852596378467291748,,96,6838,1983-06-14,\"\"\"C4CA-=; **1679; .. 79", "210,11", "908e", "1281-03-09", "7257.4654049904275", "20efe749-50fe-4b6a-a603-7f9cd1dc6c6d", "3", "New York, NY", "u", "2.228169203286535", "t"], raw_parsed_csv_data.first)
  end

  def test_only_rows
    raw_parsed_csv_data = Rcsv.raw_parse(@csv_data, :only_rows => [['GBP', 'NO SUCH THING']])

    assert_equal('GBP', raw_parsed_csv_data[0][0])
    assert_equal('15', raw_parsed_csv_data[-1][3])
    assert_equal(3, raw_parsed_csv_data.size)
  end

  def test_except_rows
    raw_parsed_csv_data = Rcsv.raw_parse(@csv_data, :except_rows => [['GBP', 'NO SUCH THING']])

    assert_equal('C85A5B9F', raw_parsed_csv_data[1][0])
    assert_equal('4971', raw_parsed_csv_data[28][4])
    assert_equal(886, raw_parsed_csv_data.size)
  end

  def test_only_rows_with_nil_and_empty_string_filter
    raw_parsed_csv_data = Rcsv.raw_parse(@csv_data, :only_rows => [['GBP'], nil, ['zzz', nil], ['51']])

    assert_equal('GBP', raw_parsed_csv_data[0][0])
    assert_equal(nil, raw_parsed_csv_data[0][2])
    assert_equal(1, raw_parsed_csv_data.size)
  end

  def test_only_rows_with_nil_beginning
    raw_parsed_csv_data = Rcsv.raw_parse(@csv_data, :only_rows => [nil, nil, nil, [123, 96], nil], :row_conversions => 'sssi')

    assert_equal('C3B87A6B', raw_parsed_csv_data[1][0])
    assert_equal(nil, raw_parsed_csv_data[0][2])
    assert_equal(96, raw_parsed_csv_data[3][3])
    assert_equal(5, raw_parsed_csv_data.size)
  end

  def test_row_defaults
    raw_parsed_csv_data = Rcsv.raw_parse(@csv_data, :row_defaults => [nil, nil, :booya, nil, 'never ever'])

    assert_equal('EDADEDADEDADEDADEDADEDAD', raw_parsed_csv_data[0][2])
    assert_equal(:booya, raw_parsed_csv_data[1][2])
    assert_equal('C85A5B9F', raw_parsed_csv_data[1][0])
    assert_equal('9134', raw_parsed_csv_data[2][4])
  end

  def test_row_conversions
    raw_parsed_csv_data = Rcsv.raw_parse(StringIO.new(@csv_data.each_line.to_a[1..-1].join), # skipping string headers
                                         :row_conversions => 'sisiisssssfsissf')

    assert_equal(nil, raw_parsed_csv_data[0][2])
    assert_equal(96, raw_parsed_csv_data[0][3])
    assert_equal(852596378467291748, raw_parsed_csv_data[0][1])
    assert_equal('908e', raw_parsed_csv_data[0][8])
    assert_equal(-9.549296585513721, raw_parsed_csv_data[1][15])
    assert_equal('2015-12-22', raw_parsed_csv_data[3][5])
  end

  def test_row_conversions_with_column_exclusions
    raw_parsed_csv_data = Rcsv.raw_parse(StringIO.new(@csv_data.each_line.to_a[1..-1].join), # skipping string headers
                                         :row_conversions => 's f issss fsis fb')

    assert_equal(nil, raw_parsed_csv_data[0][1])
    assert_equal(6838, raw_parsed_csv_data[0][2])
    assert_equal('20efe749-50fe-4b6a-a603-7f9cd1dc6c6d', raw_parsed_csv_data[0][8])
    assert_equal(true, raw_parsed_csv_data[0][12])
    assert_equal(nil, raw_parsed_csv_data[0][13])
    assert_equal('2020-12-09', raw_parsed_csv_data[4][3])
  end

  def test_offset_rows
    raw_parsed_csv_data = Rcsv.raw_parse(@csv_data, :offset_rows => 51)

    assert_equal('0FFDEA62', raw_parsed_csv_data[0][0])
    assert_equal(889 - 51, raw_parsed_csv_data.count)
  end

  def test_rows_as_hash
    raw_parsed_csv_data = Rcsv.raw_parse(@csv_data, :row_as_hash => true, :column_names => [
      'DSAdsfksjh',
      'iii ooo iii',
      'EDADEDADEDADEDADEDADEDAD',
      '111 333 555',
      'NMLKTF',
      '---==---',
      '//',
      '###',
      '0000000000',
      'Asdad bvd qwert',
      ";'''sd",
      '@@@',
      'OCTZ',
      '$$$908080',
      '"',
      'noname',
      'booleator'
    ])

    assert_equal({
      'DSAdsfksjh' => 'C85A5B9F',
      'iii ooo iii' => '852596378467291748',
      'EDADEDADEDADEDADEDADEDAD' => nil,
      '111 333 555' => '96',
      'NMLKTF' => '6838',
      '---==---' => '1983-06-14',
      '//' => '""C4CA-=; **1679; .. 79',
      '###' => '210,11',
      '0000000000' => '908e',
      'Asdad bvd qwert' => '1281-03-09',
      ";'''sd" => '7257.4654049904275',
      '@@@' => '20efe749-50fe-4b6a-a603-7f9cd1dc6c6d',
      'OCTZ' => '3',
      '$$$908080' => "New York, NY",
      '"' => 'u',
      'noname' => '2.228169203286535',
      'booleator' => 't'
    }, raw_parsed_csv_data[1])
  end

  def test_array_block_streaming
    raw_parsed_csv_data = []

    result = Rcsv.raw_parse(@csv_data) { |row|
      raw_parsed_csv_data << row
    }

    assert_equal(nil, result)
    assert_equal('EDADEDADEDADEDADEDADEDAD', raw_parsed_csv_data[0][2])
    assert_equal('$$$908080', raw_parsed_csv_data[0][13])
    assert_equal('"', raw_parsed_csv_data[0][14])
    assert_equal('true/false', raw_parsed_csv_data[0][15])
    assert_equal(nil, raw_parsed_csv_data[0][16])
    assert_equal(nil, raw_parsed_csv_data[9][2])
    assert_equal('""C81E-=; **ECCB; .. 89', raw_parsed_csv_data[3][6])
    assert_equal('Dallas, TX', raw_parsed_csv_data[888][13])
  end

  def test_hash_block_streaming
    raw_parsed_csv_data = []
    result = Rcsv.raw_parse(@csv_data, :row_as_hash => true, :column_names => [
      'DSAdsfksjh',
      'iii ooo iii',
      'EDADEDADEDADEDADEDADEDAD',
      '111 333 555',
      'NMLKTF',
      '---==---',
      '//',
      '###',
      '0000000000',
      'Asdad bvd qwert',
      ";'''sd",
      '@@@',
      'OCTZ',
      '$$$908080',
      '"',
      'noname',
      'booleator'
    ]) { |row|
      raw_parsed_csv_data << row
    }

    assert_equal(nil, result)
    assert_equal({
      'DSAdsfksjh' => 'C85A5B9F',
      'iii ooo iii' => '852596378467291748',
      'EDADEDADEDADEDADEDADEDAD' => nil,
      '111 333 555' => '96',
      'NMLKTF' => '6838',
      '---==---' => '1983-06-14',
      '//' => '""C4CA-=; **1679; .. 79',
      '###' => '210,11',
      '0000000000' => '908e',
      'Asdad bvd qwert' => '1281-03-09',
      ";'''sd" => '7257.4654049904275',
      '@@@' => '20efe749-50fe-4b6a-a603-7f9cd1dc6c6d',
      'OCTZ' => '3',
      '$$$908080' => "New York, NY",
      '"' => 'u',
      'noname' => '2.228169203286535',
      'booleator' => 't'
    }, raw_parsed_csv_data[1])
  end

  def test_nils_and_empty_strings_default
    raw_parsed_csv_data = Rcsv.raw_parse(StringIO.new(",\"\",,   ,,\n,,  \"\", \"\" ,,"))

    assert_equal([nil, '', nil, nil, nil, nil], raw_parsed_csv_data[0])
    assert_equal([nil, nil, '', '', nil, nil], raw_parsed_csv_data[1])
  end

  def test_nils_and_empty_strings_nil
    raw_parsed_csv_data = Rcsv.raw_parse(StringIO.new(",\"\",,   ,,\n,,  \"\", \"\" ,,"), :parse_empty_fields_as => :nil)

    assert_equal([nil, nil, nil, nil, nil, nil], raw_parsed_csv_data[0])
    assert_equal([nil, nil, nil, nil, nil, nil], raw_parsed_csv_data[1])
  end

  def test_nils_and_empty_strings_string
    raw_parsed_csv_data = Rcsv.raw_parse(StringIO.new(",\"\",,   ,,\n,,  \"\", \"\" ,,"), :parse_empty_fields_as => :string)

    assert_equal(['', '', '', '', '', ''], raw_parsed_csv_data[0])
    assert_equal(['', '', '', '', '', ''], raw_parsed_csv_data[1])
  end

  def test_nils_and_empty_strings_nil_or_string
    raw_parsed_csv_data = Rcsv.raw_parse(StringIO.new(",\"\",,   ,,\n,,  \"\", \"\" ,,"), :parse_empty_fields_as => :nil_or_string)

    assert_equal([nil, '', nil, nil, nil, nil], raw_parsed_csv_data[0])
    assert_equal([nil, nil, '', '', nil, nil], raw_parsed_csv_data[1])
  end
end
