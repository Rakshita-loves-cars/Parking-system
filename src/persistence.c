#include "persistence.h"
#include <sys/stat.h>

/* Simple 32-bit Fletcher-style rolling checksum for binary file validation */
static uint32_t compute_checksum(const void* data, size_t len) {
    const uint8_t* ptr = (const uint8_t*)data;
    uint32_t sum1 = 0xffff, sum2 = 0xffff;

    while (len) {
        size_t tlen = len > 359 ? 359 : len;
        len -= tlen;
        do {
            sum1 += *ptr++;
            sum2 += sum1;
        } while (--tlen);
        sum1 = (sum1 & 0xffff) + (sum1 >> 16);
        sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    }
    sum1 = (sum1 & 0xffff) + (sum1 >> 16);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    return (sum2 << 16) | sum1;
}

StatusCode save_facility_state(const ParkingFacility* facility, const char* filepath) {
    if (!facility || !filepath) return ERR_INVALID_PARAM;

    FILE* fp = fopen(filepath, "wb");
    if (!fp) return ERR_FILE_IO;

    FileHeader header;
    memset(&header, 0, sizeof(FileHeader));
    header.magic = MAGIC_HEADER;
    header.version = FILE_FORMAT_VERSION;
    header.total_slots = (uint16_t)facility->total_slots;
    strncpy(header.facility_name, facility->facility_name, MAX_NAME_LEN - 1);
    header.total_reservations_served = (uint32_t)facility->total_reservations_served;
    header.total_revenue_collected = facility->total_revenue_collected;
    header.checksum = 0; /* Will update before write */

    /* Write placeholder header */
    if (fwrite(&header, sizeof(FileHeader), 1, fp) != 1) {
        fclose(fp);
        return ERR_FILE_IO;
    }

    for (int i = 0; i < facility->total_slots; ++i) {
        const ParkingSlot* s = &facility->slots[i];
        StoredSlotRecord s_rec;
        memset(&s_rec, 0, sizeof(StoredSlotRecord));
        s_rec.slot_id = s->slot_id;
        s_rec.floor = s->floor;
        s_rec.bay_number = s->bay_number;
        s_rec.slot_cat = (int)s->slot_cat;
        s_rec.distance = s->distance_from_entrance;
        s_rec.physical_status = (int)s->physical_status;
        strncpy(s_rec.current_occupant, s->current_occupant_reg, MAX_REG_LEN - 1);
        s_rec.reservation_count = (uint16_t)s->active_reservations_count;

        if (fwrite(&s_rec, sizeof(StoredSlotRecord), 1, fp) != 1) {
            fclose(fp);
            return ERR_FILE_IO;
        }

        const ReservationNode* cur = s->reservation_head;
        while (cur != NULL) {
            StoredReservationRecord r_rec;
            memset(&r_rec, 0, sizeof(StoredReservationRecord));
            strncpy(r_rec.ticket_id, cur->ticket_id, MAX_TICKET_LEN - 1);
            strncpy(r_rec.vehicle_reg, cur->vehicle_reg, MAX_REG_LEN - 1);
            r_rec.vehicle_cat = (int)cur->vehicle_cat;
            r_rec.start_time = (int64_t)cur->start_time;
            r_rec.end_time = (int64_t)cur->end_time;
            r_rec.checked_in = cur->checked_in ? 1 : 0;
            r_rec.actual_checkin_time = (int64_t)cur->actual_checkin_time;
            r_rec.actual_checkout_time = (int64_t)cur->actual_checkout_time;
            r_rec.total_fee = cur->total_fee;

            if (fwrite(&r_rec, sizeof(StoredReservationRecord), 1, fp) != 1) {
                fclose(fp);
                return ERR_FILE_IO;
            }
            cur = cur->next;
        }
    }

    /* Compute checksum over header fields */
    header.checksum = compute_checksum(&header, sizeof(FileHeader) - sizeof(uint32_t));
    fseek(fp, 0, SEEK_SET);
    fwrite(&header, sizeof(FileHeader), 1, fp);

    fclose(fp);
    return STATUS_OK;
}

StatusCode load_facility_state(ParkingFacility* facility, const char* filepath) {
    if (!facility || !filepath) return ERR_INVALID_PARAM;

    FILE* fp = fopen(filepath, "rb");
    if (!fp) return ERR_FILE_IO;

    FileHeader header;
    if (fread(&header, sizeof(FileHeader), 1, fp) != 1) {
        fclose(fp);
        return ERR_CORRUPT_DATA;
    }

    if (header.magic != MAGIC_HEADER || header.version != FILE_FORMAT_VERSION) {
        fclose(fp);
        return ERR_CORRUPT_DATA;
    }

    /* Verify checksum */
    uint32_t computed = compute_checksum(&header, sizeof(FileHeader) - sizeof(uint32_t));
    if (computed != header.checksum) {
        fclose(fp);
        return ERR_CORRUPT_DATA;
    }

    /* Free any pre-existing facility memory */
    free_parking_facility(facility);
    memset(facility, 0, sizeof(ParkingFacility));

    strncpy(facility->facility_name, header.facility_name, MAX_NAME_LEN - 1);
    facility->total_slots = header.total_slots;
    facility->total_reservations_served = (int)header.total_reservations_served;
    facility->total_revenue_collected = header.total_revenue_collected;
    facility->default_strategy = ALLOC_GREEDY_DISTANCE;

    for (int i = 0; i < facility->total_slots; ++i) {
        StoredSlotRecord s_rec;
        if (fread(&s_rec, sizeof(StoredSlotRecord), 1, fp) != 1) {
            fclose(fp);
            return ERR_CORRUPT_DATA;
        }

        ParkingSlot* s = &facility->slots[i];
        s->slot_id = s_rec.slot_id;
        s->floor = s_rec.floor;
        s->bay_number = s_rec.bay_number;
        s->slot_cat = (VehicleCategory)s_rec.slot_cat;
        s->distance_from_entrance = s_rec.distance;
        s->physical_status = (PhysicalStatus)s_rec.physical_status;
        strncpy(s->current_occupant_reg, s_rec.current_occupant, MAX_REG_LEN - 1);
        s->reservation_head = NULL;
        s->active_reservations_count = 0;

        for (int r = 0; r < s_rec.reservation_count; ++r) {
            StoredReservationRecord r_rec;
            if (fread(&r_rec, sizeof(StoredReservationRecord), 1, fp) != 1) {
                fclose(fp);
                return ERR_CORRUPT_DATA;
            }

            ReservationNode* node = create_reservation_node(r_rec.ticket_id,
                                                           r_rec.vehicle_reg,
                                                           (VehicleCategory)r_rec.vehicle_cat,
                                                           (time_t)r_rec.start_time,
                                                           (time_t)r_rec.end_time);
            if (node) {
                node->checked_in = (r_rec.checked_in != 0);
                node->actual_checkin_time = (time_t)r_rec.actual_checkin_time;
                node->actual_checkout_time = (time_t)r_rec.actual_checkout_time;
                node->total_fee = r_rec.total_fee;

                /* Insert directly into sorted list */
                add_reservation_to_slot(s, node);
            }
        }
    }

    fclose(fp);
    return STATUS_OK;
}

StatusCode export_reservations_to_csv(const ParkingFacility* facility, const char* csv_filepath) {
    if (!facility || !csv_filepath) return ERR_INVALID_PARAM;

    FILE* fp = fopen(csv_filepath, "w");
    if (!fp) return ERR_FILE_IO;

    /* Write CSV Header */
    fprintf(fp, "TicketID,SlotID,Floor,BayNumber,Category,VehicleReg,VehicleType,StartTime,EndTime,CheckedIn,Fee\n");

    for (int i = 0; i < facility->total_slots; ++i) {
        const ParkingSlot* s = &facility->slots[i];
        const ReservationNode* cur = s->reservation_head;
        while (cur != NULL) {
            char s_buf[TIME_STR_BUFFER], e_buf[TIME_STR_BUFFER];
            format_datetime_str(cur->start_time, s_buf, sizeof(s_buf));
            format_datetime_str(cur->end_time, e_buf, sizeof(e_buf));

            fprintf(fp, "%s,%d,%d,%d,%s,%s,%s,%s,%s,%s,%.2f\n",
                    cur->ticket_id,
                    s->slot_id,
                    s->floor,
                    s->bay_number,
                    category_to_string(s->slot_cat),
                    cur->vehicle_reg,
                    category_to_string(cur->vehicle_cat),
                    s_buf,
                    e_buf,
                    cur->checked_in ? "YES" : "NO",
                    cur->total_fee);

            cur = cur->next;
        }
    }

    fclose(fp);
    return STATUS_OK;
}
