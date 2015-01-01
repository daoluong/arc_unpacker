require_relative '../../lib/renpy/rpa_archive'
require_relative '../test_helper'

# Unit tests for RpaArchive
class RpaArchiveTest < Test::Unit::TestCase
  def test_unecrypted
    TestHelper.generic_pack_and_unpack_test(RpaArchive)
  end

  def test_encrypted
    TestHelper.generic_pack_and_unpack_test(RpaArchive, key: 0x1234567)
  end
end
