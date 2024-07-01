class Bino < Formula
  desc "3D video player with support for 180/360 video and Virtual Reality"
  homepage "https://bino3d.org"
  url "https://github.com/marlam/bino.git", tag: "bino-2.2"
  license "GPL-3.0-or-later"
  head "https://github.com/marlam/bino.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "qt@6"

  def install
    system "cmake", "-S", ".", "-B", "build", "-D", "QVR_FOUND=OFF", *std_cmake_args
    system "make", "-C", "build", "-j", "4"
    system "cmake", "--install", "build"
  end

  test do
    # `test do` will create, run in and delete a temporary directory.
    #
    # This test will fail and we won't accept that! For Homebrew/homebrew-core
    # this will need to be a test that verifies the functionality of the
    # software. Run the test with `brew test bino`. Options passed
    # to `brew install` such as `--HEAD` also need to be provided to `brew test`.
    #
    # The installed folder is not in the path, so use the entire path to any
    # executables being tested: `system "#{bin}/program", "do", "something"`.
    system "false"
  end
end
