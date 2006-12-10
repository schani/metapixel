# $Id$
%define _bindir /usr/bin
%define _mandir /usr/share/man

Summary: Metapixel Photomosaic Generator
Name: metapixel
Version: 1.0.2
Release: 1
License: GNU General Public License
Group: Applications/Multimedia
URL: http://www.complang.tuwien.ac.at/schani/metapixel/
Source: http://www.complang.tuwien.ac.at/schani/%{name}/%{name}-%{version}.tar.gz
#Buildroot: %{_tmppath}/%{name}-%{version}-root
Requires: libpng
Requires: libjpeg
Requires: giflib
Requires: perl
BuildRequires: libpng-devel
BuildRequires: libjpeg-devel
BuildRequires: giflib-devel
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
install -d $RPM_BUILD_ROOT/%{_bindir}
install -d $RPM_BUILD_ROOT/%{_mandir}/man1
install metapixel $RPM_BUILD_ROOT/%{_bindir}/metapixel
install metapixel-prepare $RPM_BUILD_ROOT/%{_bindir}/metapixel-prepare
install metapixel-imagesize $RPM_BUILD_ROOT/%{_bindir}/metapixel-imagesize
install metapixel-sizesort $RPM_BUILD_ROOT/%{_bindir}/metapixel-sizesort
install metapixel.1 $RPM_BUILD_ROOT/%{_mandir}/man1/metapixel.1
gzip $RPM_BUILD_ROOT/%{_mandir}/man1/metapixel.1

%clean
rm -rf %{buildroot}

%files
%defattr(-, root, root)
%doc NEWS COPYING README
%{_mandir}/man1/metapixel.1.gz
%{_bindir}/metapixel-prepare
%{_bindir}/metapixel
%{_bindir}/metapixel-imagesize
%{_bindir}/metapixel-sizesort

%changelog
* Sun Dec 10 2006 Mark Probst <schani@complang.tuwien.ac.at> 1.0.2
- Update to 1.0.2
* Sun Feb 19 2006 Mark Probst <schani@complang.tuwien.ac.at> 1.0.1
- Update to 1.0.1, added imagesize and sizesort
* Thu Jul 28 2005 A. Pense <www.yousns.com> 1.0.0
- First creation of spec file, source rpm
