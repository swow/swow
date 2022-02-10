<?php
/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\Http;

class MimeType
{
    public const HTML = 'text/html';
    public const CSS = 'text/css';
    public const XML = 'text/xml';
    public const GIF = 'image/gif';
    public const JPEG = 'image/jpeg';
    public const JS = 'application/javascript';
    public const ATOM = 'application/atom+xml';
    public const RSS = 'application/rss+xml';
    public const MML = 'text/mathml';
    public const TXT = 'text/plain';
    public const JAD = 'text/vnd.sun.j2me.app-descriptor';
    public const WML = 'text/vnd.wap.wml';
    public const HTC = 'text/x-component';
    public const AVIF = 'image/avif';
    public const PNG = 'image/png';
    public const SVG = 'image/svg+xml';
    public const SVGZ = self::SVG;
    public const TIFF = 'image/tiff';
    public const WBMP = 'image/vnd.wap.wbmp';
    public const WEBP = 'image/webp';
    public const ICO = 'image/x-icon';
    public const JNG = 'image/x-jng';
    public const BMP = 'image/x-ms-bmp';
    public const WOFF = 'font/woff';
    public const WOFF2 = 'font/woff2';
    public const JAR = 'application/java-archive';
    public const WAR = self::JAR;
    public const EAR = self::JAR;
    public const JSON = 'application/json';
    public const HQX = 'application/mac-binhex40';
    public const DOC = 'application/msword';
    public const PDF = 'application/pdf';
    public const PS = 'application/postscript';
    public const EPS = self::PS;
    public const AI = self::PS;
    public const RTF = 'application/rtf';
    public const M3U8 = 'application/vnd.apple.mpegurl';
    public const KML = 'application/vnd.google-earth.kml+xml';
    public const KMZ = 'application/vnd.google-earth.kmz';
    public const XLS = 'application/vnd.ms-excel';
    public const EOT = 'application/vnd.ms-fontobject';
    public const PPT = 'application/vnd.ms-powerpoint';
    public const ODG = 'application/vnd.oasis.opendocument.graphics';
    public const ODP = 'application/vnd.oasis.opendocument.presentation';
    public const ODS = 'application/vnd.oasis.opendocument.spreadsheet';
    public const ODT = 'application/vnd.oasis.opendocument.text';
    public const PPTX = 'application/vnd.openxmlformats-officedocument.presentationml.presentation';
    public const XLSX = 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet';
    public const DOCX = 'application/vnd.openxmlformats-officedocument.wordprocessingml.document';
    public const WMLC = 'application/vnd.wap.wmlc';
    public const WASM = 'application/wasm';
    public const _7Z = 'application/x-7z-compressed';
    public const CCO = 'application/x-cocoa';
    public const JARDIFF = 'application/x-java-archive-diff';
    public const JNLP = 'application/x-java-jnlp-file';
    public const RUN = 'application/x-makeself';
    public const PL = 'application/x-perl';
    public const PM = self::PL;
    public const PRC = 'application/x-pilot';
    public const PDB = self::PRC;
    public const RAR = 'application/x-rar-compressed';
    public const RPM = 'application/x-redhat-package-manager';
    public const SEA = 'application/x-sea';
    public const SWF = 'application/x-shockwave-flash';
    public const SIT = 'application/x-stuffit';
    public const TCL = 'application/x-tcl';
    public const TK = self::TCL;
    public const DER = 'application/x-x509-ca-cert';
    public const PEM = self::DER;
    public const CRT = self::DER;
    public const XPI = 'application/x-xpinstall';
    public const XHTML = 'application/xhtml+xml';
    public const XSPF = 'application/xspf+xml';
    public const ZIP = 'application/zip';
    public const BIN = 'application/octet-stream';
    public const EXE = self::BIN;
    public const DLL = self::BIN;
    public const DEB = self::BIN;
    public const DMG = self::BIN;
    public const ISO = self::BIN;
    public const IMG = self::BIN;
    public const MSI = self::BIN;
    public const MSP = self::BIN;
    public const MSM = self::BIN;
    public const MIDI = 'audio/midi';
    public const MP3 = 'audio/mpeg';
    public const OGG = 'audio/ogg';
    public const M4A = 'audio/x-m4a';
    public const RA = 'audio/x-realaudio';
    public const _3GPP = 'video/3gpp';
    public const TS = 'video/mp2t';
    public const MP4 = 'video/mp4';
    public const MPEG = 'video/mpeg';
    public const MOV = 'video/quicktime';
    public const WEBM = 'video/webm';
    public const FLV = 'video/x-flv';
    public const M4V = 'video/x-m4v';
    public const MNG = 'video/x-mng';
    public const ASX = 'video/x-ms-asf';
    public const ASF = self::ASX;
    public const WMV = 'video/x-ms-wmv';
    public const AVI = 'video/x-msvideo';

    protected const EXTENSION_MAP = [
        'html' => self::HTML,
        'htm' => self::HTML,
        'shtml' => self::HTML,
        'css' => self::CSS,
        'xml' => self::XML,
        'gif' => self::GIF,
        'jpeg' => self::JPEG,
        'jpg' => self::JPEG,
        'js' => self::JS,
        'atom' => self::ATOM,
        'rss' => self::RSS,
        'mml' => self::MML,
        'txt' => self::TXT,
        'jad' => self::JAD,
        'wml' => self::WML,
        'htc' => self::HTC,
        'avif' => self::AVIF,
        'png' => self::PNG,
        'svg' => self::SVGZ,
        'svgz' => self::SVGZ,
        'tif' => self::TIFF,
        'tiff' => self::TIFF,
        'wbmp' => self::WBMP,
        'webp' => self::WEBP,
        'ico' => self::ICO,
        'jng' => self::JNG,
        'bmp' => self::BMP,
        'woff' => self::WOFF,
        'woff2' => self::WOFF2,
        'jar' => self::EAR,
        'war' => self::EAR,
        'ear' => self::EAR,
        'json' => self::JSON,
        'hqx' => self::HQX,
        'doc' => self::DOC,
        'pdf' => self::PDF,
        'ps' => self::AI,
        'eps' => self::AI,
        'ai' => self::AI,
        'rtf' => self::RTF,
        'm3u8' => self::M3U8,
        'kml' => self::KML,
        'kmz' => self::KMZ,
        'xls' => self::XLS,
        'eot' => self::EOT,
        'ppt' => self::PPT,
        'odg' => self::ODG,
        'odp' => self::ODP,
        'ods' => self::ODS,
        'odt' => self::ODT,
        'pptx' => self::PPTX,
        'xlsx' => self::XLSX,
        'docx' => self::DOCX,
        'wmlc' => self::WMLC,
        'wasm' => self::WASM,
        '7z' => self::_7Z,
        'cco' => self::CCO,
        'jardiff' => self::JARDIFF,
        'jnlp' => self::JNLP,
        'run' => self::RUN,
        'pl' => self::PM,
        'pm' => self::PM,
        'prc' => self::PDB,
        'pdb' => self::PDB,
        'rar' => self::RAR,
        'rpm' => self::RPM,
        'sea' => self::SEA,
        'swf' => self::SWF,
        'sit' => self::SIT,
        'tcl' => self::TK,
        'tk' => self::TK,
        'der' => self::CRT,
        'pem' => self::CRT,
        'crt' => self::CRT,
        'xpi' => self::XPI,
        'xhtml' => self::XHTML,
        'xspf' => self::XSPF,
        'zip' => self::ZIP,
        'bin' => self::MSM,
        'exe' => self::MSM,
        'dll' => self::MSM,
        'deb' => self::MSM,
        'dmg' => self::MSM,
        'iso' => self::MSM,
        'img' => self::MSM,
        'msi' => self::MSM,
        'msp' => self::MSM,
        'msm' => self::MSM,
        'mid' => self::MIDI,
        'midi' => self::MIDI,
        'kar' => self::MIDI,
        'mp3' => self::MP3,
        'ogg' => self::OGG,
        'm4a' => self::M4A,
        'ra' => self::RA,
        '3gpp' => self::_3GPP,
        '3gp' => self::_3GPP,
        'ts' => self::TS,
        'mp4' => self::MP4,
        'mpeg' => self::MPEG,
        'mpg' => self::MPEG,
        'mov' => self::MOV,
        'webm' => self::WEBM,
        'flv' => self::FLV,
        'm4v' => self::M4V,
        'mng' => self::MNG,
        'asx' => self::ASF,
        'asf' => self::ASF,
        'wmv' => self::WMV,
        'avi' => self::AVI,
    ];

    public static function fromExtension(string $extension): string
    {
        return self::EXTENSION_MAP[$extension] ?? static::BIN;
    }
}
