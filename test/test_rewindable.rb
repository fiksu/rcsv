require 'test/unit'
require 'rcsv/rewindable'

require 'stringio'

class RewindableTest < Test::Unit::TestCase
  def test_rewindable_supports_read
    rewindable = Rewindable.new(StringIO.new('test text'))
    assert_equal('te', rewindable.read(2))
    assert_equal('st', rewindable.read(2))
    assert_equal(0, rewindable.rewind)
    assert_equal('test text', rewindable.read)
    rewindable.close
  end

  def test_rewindable_returns_nil_on_empty_read_like_stringio
    rewindable = Rewindable.new(StringIO.new)
    assert_equal('', rewindable.read)
    assert_nil(rewindable.read(10))
    rewindable.rewind
    assert_equal('', rewindable.read)
    assert_nil(rewindable.read(10))
    rewindable.close
  end

  def test_rewindable_first_line
    rewindable = Rewindable.new(StringIO.new("foo\nbar\n"))
    assert_equal("foo\n", rewindable.first_line)
    assert_equal("foo\nbar\n", rewindable.read)
    rewindable.close
  end

  def test_rewindable_refuses_second_rewind
    rewindable = Rewindable.new(StringIO.new('test text'))
    rewindable.rewind
    assert_raise(IOError) { rewindable.rewind }
    rewindable.close
  end

  def test_rewindable_respects_encoding_after_rewind
  end
end
