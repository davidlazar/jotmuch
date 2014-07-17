To archive bookmarks anonymously, you can
[Torify](https://trac.torproject.org/projects/tor/wiki/doc/TorifyHOWTO)
urlsnap.  Unfortunately, urlsnap seems to leak DNS requests when used with
[torsocks](https://trac.torproject.org/projects/tor/wiki/doc/torsocks):

    $ torsocks urlsnap https://check.torproject.org --html /dev/stdout
    22:43:19 libtorsocks(24594): sendmsg: Connection is a UDP or ICMP stream, may be a DNS request or other form of leak: rejecting.
    ...

Alternatively, you can run urlsnap behind a
[transparent proxy](https://trac.torproject.org/projects/tor/wiki/doc/TransparentProxy).
My setup redirects traffic from the 'anonymous' user to Tor:

/etc/tor/torrc:

    TransPort 9040
    DNSPort 5353

/etc/iptables/iptables.rules:

    *nat
    -A OUTPUT -p tcp -m owner --uid-owner anonymous -m tcp -j REDIRECT --to-ports 9040
    -A OUTPUT -p udp -m owner --uid-owner anonymous -m udp --dport 53 -j REDIRECT --to-ports 5353
    COMMIT

    *filter
    -A OUTPUT -p tcp -m owner --uid-owner anonymous -m tcp --dport 9040 -j ACCEPT
    -A OUTPUT -p udp -m owner --uid-owner anonymous -m udp --dport 5353 -j ACCEPT
    -A OUTPUT -m owner --uid-owner anonymous -j DROP

    # other rules
    COMMIT

/etc/sudoers:

    # allow david to run commands as anonymous without a password
    david ALL=(anonymous) NOPASSWD: ALL

~/.xinitrc:

    # allow anonymous to use the X server
    xhost +si:localuser:anonymous


Check that the transparent proxy is working:

    $ sudo -u anonymous wget -qO- https://check.torproject.org/ | grep -i congratulations

If so, configure Jotmuch to use urlsnap as the anonymous user:

    # NO_AT_BRIDGE=1 disables annoying warnings from gtk
    URLSNAP = ['sudo', 'NO_AT_BRIDGE=1', '-u', 'anonymous', '/home/anonymous/bin/urlsnap']

    # ARCHIVE_DIR should be writeable by anonymous
    ARCHIVE_DIR = '/home/anonymous/jotmuch-archives'
