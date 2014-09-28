Name:           %{package_name}
Version:        %{package_version}
Release:        %{package_release}

License:        IPL
Provides:       adsmpipe
Group:          Applications/Backup
Summary:        Tivoli ADSM client for pipes.
Packager:       Joshua Keyes <joshua.michael.keyes@gmail.com>

ExclusiveOS:    linux
ExclusiveArch:  i686 x86_64
BuildRoot:      %{buildroot}/BUILDROOT

%description
ADSMPipe is a backup client for TSM that enables backing up from standard output. It's useful for
backing up or archiving databases not supported by the Tivoli Data Protection product class.

%prep
tar --strip-components=1 -xzf %{_topdir}/SOURCES/%{package_name}-%{package_version}-%{package_release}.tar.gz

%build
make PLATFORM=linux all

%install
make PLATFORM=linux DESTDIR=%{buildroot} PREFIX=/usr install

%files
%defattr(0755,root,root)
%_bindir/adsmpipe
%_datadir/adsmpipe/mysql.backup.sh
%_datadir/adsmpipe/mysql.restore.sh
%defattr(0644,root,root)
%doc %_mandir/man1/adsmpipe.1.gz
