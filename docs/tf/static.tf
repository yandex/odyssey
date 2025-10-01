locals {
  folder_id = "b1g05ic72g99lca65o8q"
  sa_id = "ajekb8uit552vive76k5"
  domain    = "odyssey.yandex"
}

terraform {
  required_providers {
    yandex = {
      source  = "yandex-cloud/yandex"
      version = ">= 0.47.0"
    }
  }
}

provider "yandex" {
  folder_id = local.folder_id
}

resource "yandex_iam_service_account_static_access_key" "sa-static-key" {
  service_account_id = local.sa_id
  description        = "static access key for object storage"
}

resource "yandex_storage_bucket" "odyssey-web" {
  access_key = yandex_iam_service_account_static_access_key.sa-static-key.access_key
  secret_key = yandex_iam_service_account_static_access_key.sa-static-key.secret_key
  bucket     = local.domain
  max_size   = 1073741824
  acl        = "public-read"

  website {
    index_document = "index.html"
    error_document = "404.html"
  }

  https {
    certificate_id = data.yandex_cm_certificate.example.id
  }
}

resource "yandex_cm_certificate" "le-certificate" {
  name    = "my-le-cert"
  domains = ["${local.domain}"]

  managed {
    challenge_type = "DNS_CNAME"
  }
}

resource "yandex_dns_recordset" "validation-record" {
  zone_id = yandex_dns_zone.zone1.id
  name    = yandex_cm_certificate.le-certificate.challenges[0].dns_name
  type    = yandex_cm_certificate.le-certificate.challenges[0].dns_type
  data    = [yandex_cm_certificate.le-certificate.challenges[0].dns_value]
  ttl     = 600
}

data "yandex_cm_certificate" "example" {
  depends_on      = [yandex_dns_recordset.validation-record]
  certificate_id  = yandex_cm_certificate.le-certificate.id
  #wait_validation = true
}

resource "yandex_dns_zone" "zone1" {
  name        = "odyssey-zone-1"
  description = "Public zone"
  zone        = "${local.domain}."
  public      = true
}

resource "yandex_dns_recordset" "rs2" {
  zone_id = yandex_dns_zone.zone1.id
  name    = "${local.domain}."
  type    = "ANAME"
  ttl     = 600
  data    = ["${local.domain}.website.yandexcloud.net"]
}
