% snb-core(1)
% SuperBlockChain Development Foundation
%

# NAME

snb-core - Core daemon for SuperBlockChain payment network

# SYNOPSYS

snb-core [OPTIONS]

# DESCRIPTION

SuperBlockChain is a decentralized, federated peer-to-peer network that allows
people to send payments in any asset anywhere in the world
instantaneously, and with minimal fee. `SuperBlockChain-core` is the core
component of this network. `SuperBlockChain-core` is a C++ implementation of
the SuperBlockChain Consensus Protocol configured to construct a chain of
ledgers that are guaranteed to be in agreement across all the
participating nodes at all times.

## Configuration file

In most modes of operation, snb-core requires a configuration
file.  By default, it looks for a file called `snb-core.cfg` in
the current working directory, but this default can be changed by the
`--conf` command-line option.  The configuration file is in TOML
syntax.  The full set of supported directives can be found in
`%prefix%/share/doc/snb-core_example.cfg`.

%commands%

# EXAMPLES

See `%prefix%/share/doc/*.cfg` for some example snb-core
configuration files

# FILES

snb-core.cfg
:   Configuration file (in current working directory by default)

# SEE ALSO

<https://www.snb.org/developers/snb-core/software/admin.html>
:   snb-core administration guide

<https://www.snb.org>
:   Home page of SuperBlockChain development foundation

# BUGS

Please report bugs using the github issue tracker:\
<https://github.com/snb/snb-core/issues>
