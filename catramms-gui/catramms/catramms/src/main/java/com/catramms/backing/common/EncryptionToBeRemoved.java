package com.catramms.backing.common;

import org.apache.log4j.Logger;

import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;
import java.security.Key;
import java.util.Base64;

/**
 * Created by multi on 22.03.17.
 */
public class EncryptionToBeRemoved {

    private static final Logger mLogger = Logger.getLogger(EncryptionToBeRemoved.class);

    private String key = "Bar12345Bar12345"; // 128 bit key

    public String encrypt(String textToEncrypt)
    {
        String textEncrypted = null;

        try
        {
            // Create key and cipher
            Key aesKey = new SecretKeySpec(key.getBytes(), "AES");
            Cipher cipher = Cipher.getInstance("AES");

            // encrypt the text
            cipher.init(Cipher.ENCRYPT_MODE, aesKey);
            byte[] encrypted = cipher.doFinal(textToEncrypt.getBytes());

            textEncrypted = Base64.getEncoder().encodeToString(encrypted); // new String(encrypted);

            mLogger.info("textToEncrypt: " + textToEncrypt + ", textEncrypted: " + textEncrypted);
        }
        catch (Exception e)
        {
            mLogger.error("encrypt failed. Exception: " + e);

            textEncrypted = null;
        }

        return textEncrypted;
    }

    public String decrypt(String textEncrypted)
    {
        String textDecrypted = null;

        try
        {
            // Create key and cipher
            Key aesKey = new SecretKeySpec(key.getBytes(), "AES");
            Cipher cipher = Cipher.getInstance("AES");

            cipher.init(Cipher.DECRYPT_MODE, aesKey);
            // textDecrypted = new String(cipher.doFinal(textEncrypted.getBytes()));
            textDecrypted = new String(cipher.doFinal(Base64.getDecoder().decode(textEncrypted)));

            mLogger.info("textEncrypted: " + textEncrypted + ", textDecrypted: " + textDecrypted);
        }
        catch (Exception e)
        {
            mLogger.error("decrypt failed. Exception: " + e);

            textDecrypted = null;
        }

        return textDecrypted;
    }
}
