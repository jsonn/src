//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007, 2008 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#if !defined(_ATF_CXX_FS_HPP_)
#define _ATF_CXX_FS_HPP_

extern "C" {
#include <sys/types.h>
}

#include <map>
#include <set>
#include <stdexcept>
#include <string>

extern "C" {
#include "atf-c/fs.h"
}

namespace atf {
namespace fs {

// ------------------------------------------------------------------------
// The "path" class.
// ------------------------------------------------------------------------

//!
//! \brief A class to represent a path to a file.
//!
//! The path class represents the route to a file or directory in the
//! file system.  All file manipulation operations use this class to
//! represent their arguments as it takes care of normalizing user-provided
//! strings and ensures they are valid.
//!
//! It is important to note that the file pointed to by a path need not
//! exist.
//!
class path {
    //!
    //! \brief Internal representation of a path.
    //!
    atf_fs_path_t m_path;

public:
    //! \brief Constructs a new path from a user-provided string.
    //!
    //! This constructor takes a string, either provided by the program's
    //! code or by the user and constructs a new path object.  The string
    //! is normalized to not contain multiple delimiters together and to
    //! remove any trailing one.
    //!
    //! The input string cannot be empty.
    //!
    explicit path(const std::string&);

    //!
    //! \brief Copy constructor.
    //!
    path(const path&);

    //!
    //! \brief Destructor for the path class.
    //!
    ~path(void);

    //!
    //! \brief Returns a pointer to a C-style string representing this path.
    //!
    const char* c_str(void) const;

    //!
    //! \brief Returns a pointer to the implementation data.
    //!
    const atf_fs_path_t* c_path(void) const;

    //!
    //! \brief Returns a string representing this path.
    //! XXX Really needed?
    //!
    std::string str(void) const;

    //!
    //! \brief Returns the branch path of this path.
    //!
    //! Calculates and returns the branch path of this path.  In other
    //! words, it returns what the standard ::dirname function would return.
    //!
    path branch_path(void) const;

    //!
    //! \brief Returns the leaf name of this path.
    //!
    //! Calculates and returns the leaf name of this path.  In other words,
    //! it returns what the standard ::basename function would return.
    //!
    std::string leaf_name(void) const;

    //!
    //! \brief Checks whether this path is absolute or not.
    //!
    //! Returns a boolean indicating if this is an absolute path or not;
    //! i.e. if it starts with a slash.
    //!
    bool is_absolute(void) const;

    //!
    //! \brief Checks whether this path points to the root directory or not.
    //!
    //! Returns a boolean indicating if this is path points to the root
    //! directory or not.  The checks made by this are extremely simple (so
    //! the results cannot always be trusted) but they are enough for our
    //! modest sanity-checking needs.  I.e. "/../" could return false.
    //!
    bool is_root(void) const;

    //!
    //! \brief Converts the path to be absolute.
    //!
    //! \pre The path was not absolute.
    //!
    path to_absolute(void) const;

    //!
    //! \brief Assignment operator.
    //!
    path& operator=(const path&);

    //!
    //! \brief Checks if two paths are equal.
    //!
    bool operator==(const path&) const;

    //!
    //! \brief Checks if two paths are different.
    //!
    bool operator!=(const path&) const;

    //!
    //! \brief Concatenates a path with a string.
    //!
    //! Constructs a new path object that is the concatenation of the
    //! left-hand path with the right-hand string.  The string is normalized
    //! before the concatenation, and a path delimiter is introduced between
    //! the two components if needed.
    //!
    path operator/(const std::string&) const;

    //!
    //! \brief Concatenates a path with another path.
    //!
    //! Constructs a new path object that is the concatenation of the
    //! left-hand path with the right-hand one. A path delimiter is
    //! introduced between the two components if needed.
    //!
    path operator/(const path&) const;

    //!
    //! \brief Checks if a path has to be sorted before another one
    //!        lexicographically.
    //!
    bool operator<(const path&) const;
};

// ------------------------------------------------------------------------
// The "file_info" class.
// ------------------------------------------------------------------------

class directory;

//!
//! \brief A class that contains information about a file.
//!
//! The file_info class holds information about an specific file that
//! exists in the file system.
//!
class file_info {
    atf_fs_stat_t m_stat;

public:
    //!
    //! \brief The file's type.
    //!
    static const int blk_type;
    static const int chr_type;
    static const int dir_type;
    static const int fifo_type;
    static const int lnk_type;
    static const int reg_type;
    static const int sock_type;
    static const int wht_type;

    //!
    //! \brief Constructs a new file_info based on a given file.
    //!
    //! This constructor creates a new file_info object and fills it with
    //! the data returned by ::stat when run on the given file, which must
    //! exist.
    //!
    explicit file_info(const path&);

    //!
    //! \brief The copy constructor.
    //!
    file_info(const file_info&);

    //!
    //! \brief The destructor.
    //!
    ~file_info(void);

    //!
    //! \brief Returns the device containing the file.
    //!
    dev_t get_device(void) const;

    //!
    //! \brief Returns the file's inode.
    //!
    ino_t get_inode(void) const;

    //!
    //! \brief Returns the file's type.
    //!
    int get_type(void) const;

    //!
    //! \brief Returns whether the file is readable by its owner or not.
    //!
    bool is_owner_readable(void) const;

    //!
    //! \brief Returns whether the file is writable by its owner or not.
    //!
    bool is_owner_writable(void) const;

    //!
    //! \brief Returns whether the file is executable by its owner or not.
    //!
    bool is_owner_executable(void) const;

    //!
    //! \brief Returns whether the file is readable by the users belonging
    //! to its group or not.
    //!
    bool is_group_readable(void) const;

    //!
    //! \brief Returns whether the file is writable the users belonging to
    //! its group or not.
    //!
    bool is_group_writable(void) const;

    //!
    //! \brief Returns whether the file is executable by the users
    //! belonging to its group or not.
    //!
    bool is_group_executable(void) const;

    //!
    //! \brief Returns whether the file is readable by people different
    //! than the owner and those belonging to the group or not.
    //!
    bool is_other_readable(void) const;

    //!
    //! \brief Returns whether the file is write by people different
    //! than the owner and those belonging to the group or not.
    //!
    bool is_other_writable(void) const;

    //!
    //! \brief Returns whether the file is executable by people different
    //! than the owner and those belonging to the group or not.
    //!
    bool is_other_executable(void) const;
};

// ------------------------------------------------------------------------
// The "directory" class.
// ------------------------------------------------------------------------

//!
//! \brief A class representing a file system directory.
//!
//! The directory class represents a group of files in the file system and
//! corresponds to exactly one directory.
//!
class directory : public std::map< std::string, file_info > {
public:
    //!
    //! \brief Constructs a new directory.
    //!
    //! Constructs a new directory object representing the given path.
    //! The directory must exist at creation time as the contents of the
    //! class are gathered from it.
    //!
    directory(const path&);

    //!
    //! \brief Returns the file names of the files in the directory.
    //!
    //! Returns the leaf names of all files contained in the directory.
    //! I.e. the keys of the directory map.
    //!
    std::set< std::string > names(void) const;
};

// ------------------------------------------------------------------------
// The "temp_dir" class.
// ------------------------------------------------------------------------

//!
//! \brief A RAII model for temporary directories.
//!
//! The temp_dir class provides a RAII model for temporary directories.
//! During construction, a safe temporary directory is created and during
//! destruction it is carefully removed by making use of the cleanup
//! function.
//!
class temp_dir {
    //!
    //! \brief The path to this temporary directory.
    //!
    path *m_path;

public:
    //!
    //! \brief Creates a new temporary directory.
    //!
    //! Creates a new temporary directory based on the provided name
    //! template.  The template must end with six X characters preceded
    //! by a dot.  These characters are replaced with a unique name on
    //! the file system as described in mkdtemp(3).
    //!
    temp_dir(const path&);

    //!
    //! \brief Destroys the temporary directory.
    //!
    //! Destroys this temporary directory object as well as its file
    //! system representation.
    //!
    ~temp_dir(void);

    //!
    //! \brief Returns the path to this temporary directory.
    //!
    const path& get_path(void) const;
};

// ------------------------------------------------------------------------
// Free functions.
// ------------------------------------------------------------------------

//!
//! \brief Changes the current working directory.
//!
//! Changes the current working directory to the given path.  Returns the
//! path to the directory just left.
//!
//! \throw system_error If ::chdir failed.
//!
path change_directory(const path&);

//!
//! \brief Checks if the given path exists.
//!
bool exists(const path&);

//!
//! \brief Looks for the given program in the PATH.
//!
//! Given a program name (without slashes) looks for it in the path and
//! returns its full path name if found, otherwise an empty path.
//!
bool have_prog_in_path(const std::string&);

//!
//! \brief Returns the path to the current working directory.
//!
//! Calculates and returns the path to the current working directory, which
//! is guessed using the ::getcwd function.
//!
//! \throw system_error If ::getcwd failed.
//!
path get_current_dir(void);

//!
//! \brief Checks if the given path exists, is accessible and is executable.
//!
bool is_executable(const path&);

//!
//! \brief Removes a given file.
//!
void remove(const path&);

//!
//! \brief Recursively cleans up a directory.
//!
//! This function cleans up a directory hierarchy.  First of all, it looks
//! for any file system that may be mounted under the given path and, if
//! any is found, an attempt is made to unmount it.  Later on, the
//! directory is removed alongside all of its contents.
//!
void cleanup(const path&);

} // namespace fs
} // namespace atf

#endif // !defined(_ATF_CXX_FS_HPP_)
