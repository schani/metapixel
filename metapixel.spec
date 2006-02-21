# $Id$
%define _bindir /usr/local/bin
%define _mandir /usr/local/man

Summary: Metapixel Photomosaic Generator
Name: metapixel
Version: 1.0.1
Release: 1
License: GNU General Public License
Group: Applications/Multimedia
URL: http://www.complang.tuwien.ac.at/schani/metapixel/
Source0: %{name}-%{version}.tar.gz
Buildroot: %{_tmppath}/%{name}-%{version}-root
Requires: libpng
Requires: libjpeg
Requires: perl
BuildRequires: make

%description
Metapixel is a program for generating photomosaics.  It can generate classical 
photomosaics, in which the source image is viewed as a matrix of equally sized 
rectangles for each of which a matching image is substituted, as well as 
collage-style photomosaics, in which rectangular parts of the source image 
at arbitrary positions (i.e. not aligned to a matrix) are substituted by 
matching images.

%prep
%setup
rm -rf $RPM_BUILD_ROOT

%build
%{__make}

%install
install -d $RPM_BUILD_ROOT/usr/local/bin
install -d $RPM_BUILD_ROOT/usr/local/man/man1
install metapixel $RPM_BUILD_ROOT/usr/local/bin/metapixel
install metapixel-prepare $RPM_BUILD_ROOT/usr/local/bin/metapixel-prepare
install metapixel-imagesize $RPM_BUILD_ROOT/usr/local/bin/metapixel-imagesize
install metapixel-sizesort $RPM_BUILD_ROOT/usr/local/bin/metapixel-sizesort
install metapixel.1 $RPM_BUILD_ROOT/usr/local/man/man1/metapixel.1

%clean
rm -rf %{buildroot}

%files
%defattr(0755, root, root)
%doc NEWS COPYING README
%{_mandir}/man1/metapixel.1
%{_bindir}/metapixel-prepare
%{_bindir}/metapixel
%{_bindir}/metapixel-imagesize
%{_bindir}/metapixel-sizesort

%changelog
* Sun Feb 19 2006 Mark Probst <schani@complang.tuwien.ac.at> 1.0.1
- Update to 1.0.1, added imagesize and sizesort
* Thu Jul 28 2005 A. Pense <www.yousns.com> 1.0.0
- First creation of spec file, source rpm
