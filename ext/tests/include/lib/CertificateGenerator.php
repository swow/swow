<?php

// from php-src ext/openssl/tests/CertificateGenerator.inc

class CertificateGenerator
{
    const CONFIG = __DIR__. DIRECTORY_SEPARATOR . 'openssl.cnf';

    /** @var OpenSSLCertificate|false */
    private $ca = false;

    /** @var OpenSSLAsymmetricKey|false */
    private $caKey = false;

    /** @var bool */
    private $useSelfSignedCert;

    /** @var OpenSSLCertificate|null */
    private $lastCert;

    /** @var OpenSSLAsymmetricKey|null */
    private $lastKey;

    public function __construct(bool $useSelfSignedCert = false)
    {
        if (!extension_loaded('openssl')) {
            throw new RuntimeException(
                'openssl extension must be loaded to generate certificates'
            );
        }
        $this->useSelfSignedCert = $useSelfSignedCert;

        if (!$this->useSelfSignedCert) {
            $this->generateCa();
        }
    }

    /**
     * @param string $curve
     * @return resource
     */
    private static function generateKey($curve = 'prime256v1')
    {
        return openssl_pkey_new([
            'ec' => [
                'curve_name' => $curve,
            ],
            'private_key_type' => OPENSSL_KEYTYPE_EC,
            'encrypt_key' => false,
        ]);
    }

    private function generateCa()
    { 
        $this->caKey = self::generateKey();
        $dn = [
            'countryName' => 'GB',
            'stateOrProvinceName' => 'Berkshire',
            'localityName' => 'Newbury',
            'organizationName' => 'Example Certificate Authority',
            'commonName' => 'CA for PHP Tests'
        ];

        $csr = openssl_csr_new($dn, $this->caKey, ['config' => self::CONFIG]);
        $this->ca = openssl_csr_sign($csr, null, $this->caKey, 365, ['config' => self::CONFIG]);
    }

    public function getCaCert()
    {
        if ($this->useSelfSignedCert) {
            throw new RuntimeException("CA is not generated in self-signed mode.");
        }

        $output = '';
        openssl_x509_export($this->ca, $output);
        return $output;
    }

    public function saveCaCert($file)
    {
        if ($this->useSelfSignedCert) {
            throw new RuntimeException("CA is not available in self-signed mode.");
        }

        openssl_x509_export_to_file($this->ca, $file);
    }

    private function generateCertAndKey($commonNameForCert, $file, $curve = 'prime256v1', $subjectAltName = null, $extKeyUsage = null)
    {
        $dn = [
            'countryName' => 'BY',
            'stateOrProvinceName' => 'Minsk',
            'localityName' => 'Minsk',
            'organizationName' => 'Example Org',
        ];
        if ($commonNameForCert !== null) {
            $dn['commonName'] = $commonNameForCert;
        }

        $subjectAltNameConfig = $subjectAltName ? "subjectAltName = $subjectAltName" : "";
        $extKeyUsageConfig = $extKeyUsage ? "extendedKeyUsage = $extKeyUsage" : "";
        $configCode = <<<CONFIG
[ req ]
distinguished_name = req_distinguished_name
default_md = sha256
default_bits = 2048

[ req_distinguished_name ]

[ v3_req ]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
$subjectAltNameConfig
$extKeyUsageConfig

[ usr_cert ]
basicConstraints = CA:FALSE
$subjectAltNameConfig
$extKeyUsageConfig
CONFIG;
        $configFile = $file . '.cnf';
        file_put_contents($configFile, $configCode);

        $config = [
            'config' => $configFile,
            'req_extensions' => 'v3_req',
            'x509_extensions' => 'usr_cert',
        ];

        $this->lastKey = self::generateKey($curve);
        $csr = openssl_csr_new($dn, $this->lastKey, $config);

        // If in self-signed mode, sign with the same key, otherwise use CA
        $signingCert = $this->useSelfSignedCert ? null : $this->ca;
        $signingKey = $this->useSelfSignedCert ? $this->lastKey : $this->caKey;

        $this->lastCert = openssl_csr_sign(
            $csr,
            $signingCert,
            $signingKey,
            365, // 1 year validity
            $config
        );

        return $config;
    }

    public function saveNewCertAsFileWithKey(
        $commonNameForCert, $file, $curve = 'prime256v1', $subjectAltName = null, $extKeyUsage = null
    ) {
        $config = $this->generateCertAndKey($commonNameForCert, $file, $curve, $subjectAltName, $extKeyUsage);

        $certText = '';
        openssl_x509_export($this->lastCert, $certText);

        $keyText = '';
        openssl_pkey_export($this->lastKey, $keyText, null, $config);

        file_put_contents($file, $certText . PHP_EOL . $keyText);

        unlink($config['config']);
    }

    public function saveNewCertAndKey(
        $commonNameForCert, $certFile, $keyFile, $keyLength = null, $subjectAltName = null, $extKeyUsage = null
    ) {
        $config = $this->generateCertAndKey($commonNameForCert, $certFile, $keyLength, $subjectAltName, $extKeyUsage);

        openssl_x509_export_to_file($this->lastCert, $certFile);
        openssl_pkey_export_to_file($this->lastKey, $keyFile, null, $config);

        unlink($config['config']);
    }

    public function saveNewCertAndPubKey(
        $commonNameForCert, $certFile, $pubKeyFile, $keyLength = null, $subjectAltName = null, $extKeyUsage = null
    ) {
        $config = $this->generateCertAndKey($commonNameForCert, $certFile, $keyLength, $subjectAltName, $extKeyUsage);

        openssl_x509_export_to_file($this->lastCert, $certFile);

        $keyDetails = openssl_pkey_get_details($this->lastKey);
        if ($keyDetails === false || !isset($keyDetails['key'])) {
            throw new RuntimeException("Failed to extract public key.");
        }

        file_put_contents($pubKeyFile, $keyDetails['key']);
        unlink($config['config']);
    }

    public function getCertDigest($algo)
    {
        return openssl_x509_fingerprint($this->lastCert, $algo);
    }
}
