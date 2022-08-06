#include <C61JCAM/Logging.hpp>

void LOGGING::initSPI()
{
    SPIC1.begin(VSPI, SCK1, MISO1, MOSI1);
    mpu9250.begin(&SPIC1, MPUCS, SPIFREQ);
    lps22hb.begin(&SPIC1, LPSCS, SPIFREQ);
    flash1.begin(&SPIC1, flashCS, SPIFREQ);
    initVariables();
}

void LOGGING::initVariables()
{
    isAttachingData = 0;
    latestDatasetIndex = 0;
    latestFlashPage = 0;
    lps_counter = 0;
    isFlashErased = 0;
    isLoggingGoing = 0;
}

IRAM_ATTR int LOGGING::checkSensor()
{
    if ((lps22hb.WhoImI() == 177) && (mpu9250.WhoImI() == 113))
    {
        return 0;
    }
    Serial.print("LPS:");
    Serial.println(lps22hb.WhoImI());
    Serial.print("MPU:");
    Serial.println(mpu9250.WhoImI());
    return 1;
}

IRAM_ATTR void LOGGING::eraseFlash()
{
    if (!isFlashErased)
    {
        flash1.erase();
    }
    isFlashErased = 1;
}

void LOGGING::readAllFlash()
{
    for (int j = 0; j < 65536; j++)
    {
        unsigned char readbuffer[DATASETSIZE];
        flash1.read(j * DATASETSIZE, readbuffer);
        for (int i = 0; i < DATASETSIZE; i++)
        {
            Serial.print(readbuffer[i]);
            Serial.print(',');
        }
    }
}

IRAM_ATTR int LOGGING::makequeue()
{
    flashQueue = xQueueCreate(FLASHQUEUELENGTH, sizeof(uint8_t *));
    if (flashQueue == NULL)
    {
        return 1;
    }
    return 0;
}

IRAM_ATTR void LOGGING::deletequeue()
{
    vQueueDelete(flashQueue);
}

IRAM_ATTR uint8_t *LOGGING::allocDataSet()
{
    uint8_t *dataset = (uint8_t *)malloc(sizeof(uint8_t) * DATASETSIZE);
    return dataset;
}

IRAM_ATTR void LOGGING::freeDataSet(uint8_t *dataset)
{
    free(dataset);
}

IRAM_ATTR int LOGGING::attachDataSet(uint8_t *data, uint8_t dataLength)
{
    while (isAttachingData)
    {
        delayMicroseconds(1);
    }
    isAttachingData = 1;
    for (int i = 0; i < dataLength; i++)
    {
        if (latestDatasetIndex == 256)
        {
            xQueueSend(flashQueue, latestDataset, 0);
            latestDataset = allocDataSet();
            if (latestDataset == NULL)
            {
                return 1;
            }
            latestDatasetIndex = 0;
        }
        latestDataset[latestDatasetIndex] = data[i];
        latestDatasetIndex++;
    }
    isAttachingData = 0;
    return 0;
}

IRAM_ATTR void LOGGING::writeFlashFromQueue()
{
    uint8_t *dataset;
    if (xQueueReceive(flashQueue, &dataset, 0) == pdTRUE)
    {
        flash1.write(latestFlashPage * 256, dataset);
        latestFlashPage++;
        freeDataSet(dataset);
    }
}

IRAM_ATTR void LOGGING::loggingData(void *parameters)
{
    portTickType xLastWakeTime = xTaskGetTickCount();
    for (;;)
    {
        // get mpu data
        int16_t mpudata[6];
        uint8_t mpudata_flashbf[17];
        unsigned long mpugettime = micros();
        mpu9250.Get(mpudata);

        // set header to bf
        mpudata_flashbf[0] = MPUDATAHEAD;
        // set time to bf
        for (int i = 0; i < 4; i++)
        {
            mpudata_flashbf[1 + i] = (unsigned char)((mpugettime >> (8 * i)) % 256);
        }
        // set mpu data to bf
        for (int i = 0; i < 6; i++)
        {
            mpudata_flashbf[5 + 2 * i] = (unsigned char)(mpudata[i] % 256);
            mpudata_flashbf[6 + 2 * i] = (unsigned char)(mpudata[i] >> 8);
        }

        // set freq(lps)
        if (lps_counter++ == MPULPSRATIO)
        {
            // lps logging task
            unsigned char lpsdata[3];
            unsigned long lpsgettime = micros();
            lps22hb.Get(lpsdata);

            // buffer
            uint8_t lpsdata_flashbf[8];

            // set headder to buffer
            lpsdata_flashbf[0] = LPSDATAHEAD;
            // set time to buffer
            for (int i = 0; i < 4; i++)
            {
                lpsdata_flashbf[1 + i] = (unsigned char)((lpsgettime >> (8 * i)) % 256);
            }
            // set lpsdata to buffer
            for (int i = 0; i < 3; i++)
            {
                lpsdata_flashbf[5 + i] = lpsdata[i];
            }
            attachDataSet(lpsdata_flashbf, 8);
            lps_counter = 0;
        }

        attachDataSet(mpudata_flashbf, 17);

        writeFlashFromQueue();

        vTaskDelayUntil(&xLastWakeTime, LOGGINGINTERVAL / portTICK_PERIOD_MS);
    }
}

IRAM_ATTR void LOGGING::logTaskCreate()
{
    if (isLoggingGoing)
    {
        return;
    }
    initVariables();
    isLoggingGoing = 1;
    makequeue();
    xTaskCreate(loggingData, "Logging", 8192, NULL, 1, &LoggingHandle);
}

IRAM_ATTR void LOGGING::logTaskDelete()
{
    if (!isLoggingGoing)
    {
        return;
    }
    vTaskDelete(LoggingHandle);
    deletequeue();
    isLoggingGoing = 0;
}