using System.Collections.Generic;
using Pulumi;
using Pulumi.Aws.Ec2;
using Pulumi.Aws.Ec2.Inputs;
using Pulumi.Command.Remote;
using Pulumi.Command.Remote.Inputs;
using Aws = Pulumi.Aws;

return await Deployment.RunAsync(() =>
{
    var config = new Config();

    // ---------------------------------------------------------------------
    // 설정값 (pulumi config set 으로 지정)
    //   pulumi config set instanceType t3.small
    //   pulumi config set sshPublicKey "ssh-ed25519 AAAA... your-key-comment"
    //   pulumi config set sshAllowedCidr "0.0.0.0/0"   # 나중에 러너 IP로 제한 권장
    // ---------------------------------------------------------------------
    var instanceType = config.Get("aws-ec2-type") ?? "t3.small";
    var sshPublicKey = config.RequireSecret("aws-ssh-public");
    var sshPrivateKey = config.RequireSecret("aws-ssh-private");
    var sshAllowedCidr = config.Get("sshAllowedCidr") ?? "0.0.0.0/0";
    var eipId = config.Get("aws-eip-id") ?? "[Set EIP id]";

    // ---------------------------------------------------------------------
    // 최신 Ubuntu 24.04 (Noble) AMI 조회 (Canonical 공식 계정 owner id)
    // ---------------------------------------------------------------------
    var ubuntuAmi = Aws.Ec2.GetAmi.Invoke(new Aws.Ec2.GetAmiInvokeArgs
    {
        MostRecent = true,
        Owners = new List<string> { "099720109477" }, // Canonical owner id
        Filters = new List<Aws.Ec2.Inputs.GetAmiFilterInputArgs>
        {
            new()
            {
                Name = "name",
                Values = new List<string> { "ubuntu/images/hvm-ssd-gp3/ubuntu-noble-24.04-amd64-server-*" },
            },
            new()
            {
                Name = "virtualization-type",
                Values = new List<string> { "hvm" },
            },
            new()
            {
                Name = "root-device-type",
                Values = new List<string> { "ebs" },
            },
        },
    });

    // ---------------------------------------------------------------------
    // SSH 키페어 (개인키는 로컬에 보관, 여기서는 공개키만 등록)
    // ---------------------------------------------------------------------
    var keyPair = new KeyPair("pulumi-ssh-keypair", new KeyPairArgs
    {
        PublicKey = sshPublicKey,
    });

    // ---------------------------------------------------------------------
    // 보안 그룹: SSH(22) 인바운드 + 전체 아웃바운드 허용
    // ---------------------------------------------------------------------
    var secGroup = new SecurityGroup("pulumi-sg", new SecurityGroupArgs
    {
        Description = "Allow SSH inbound, all outbound",
        Ingress = new[]
        {
            new SecurityGroupIngressArgs
            {
                Protocol = "tcp",
                FromPort = 22,
                ToPort = 22,
                CidrBlocks = new[] { sshAllowedCidr },
                Description = "SSH",
            },
            new SecurityGroupIngressArgs
            {
                Protocol = "tcp",
                FromPort = 51010,
                ToPort = 51010,
                CidrBlocks = new[] { "0.0.0.0/0" },
                Description = "GameServerPort",
            },
        },
        Egress = new[]
        {
            new SecurityGroupEgressArgs
            {
                Protocol = "-1",
                FromPort = 0,
                ToPort = 0,
                CidrBlocks = new[] { "0.0.0.0/0" },
            },
        },
        Tags = new Dictionary<string, string>
        {
            ["Name"] = "ec2-ssh-sg",
        },
    });

    // ---------------------------------------------------------------------
    // UserData: 부팅 시 Docker 설치 (Ubuntu 공식 apt 저장소 사용)
    // ---------------------------------------------------------------------
    const string userData = @"#!/bin/bash
set -eux

apt-get update -y
apt-get install -y ca-certificates curl

install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
chmod a+r /etc/apt/keyrings/docker.asc

tee /etc/apt/sources.list.d/docker.sources <<EOF
Types: deb
URIs: https://download.docker.com/linux/ubuntu
Suites: $(. /etc/os-release && echo ""$UBUNTU_CODENAME"")
Components: stable
Signed-By: /etc/apt/keyrings/docker.asc
EOF

apt-get update -y
apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

usermod -aG docker ubuntu
newgrp docker

systemctl enable docker
systemctl start docker
";

    var normalizedUserData = userData.Replace("\r\n", "\n");

    // ---------------------------------------------------------------------
    // EC2 인스턴스
    // ---------------------------------------------------------------------
    var instance = new Instance("isolation-server", new InstanceArgs
    {
        InstanceType = instanceType,
        Ami = ubuntuAmi.Apply(a => a.Id),
        KeyName = keyPair.KeyName,
        VpcSecurityGroupIds = new[] { secGroup.Id },
        UserData = normalizedUserData,
        AssociatePublicIpAddress = true,
        RootBlockDevice = new InstanceRootBlockDeviceArgs
        {
            VolumeSize = 20,
            VolumeType = "gp3",
        },
        Tags = new Dictionary<string, string>
        {
            ["wakeup"] = "weekday",
        },
    });

    // ---------------------------------------------------------------------
    // Elastic IP 연결
    // ---------------------------------------------------------------------
    var eipAssoc = new Aws.Ec2.EipAssociation("isolation-server-eip-association", new()
    {
        InstanceId = instance.Id,
        AllocationId = eipId,
    });
    
    // ---------------------------------------------------------------------
    // SSH 접속 후 초기화 완료 확인
    // ---------------------------------------------------------------------
    var waitForInstallDocker = new Command("wait-for-docker", new CommandArgs
        {
            Connection = new ConnectionArgs
            {
                Host = eipAssoc.PublicIp,
                User = "ubuntu",
                PrivateKey = sshPrivateKey,
            },
            Create = @"
                for i in $(seq 1 30); do
                  if command -v docker >/dev/null 2>&1 && systemctl is-active --quiet docker; then
                    echo 'docker ready'
                    exit 0
                  fi
                  sleep 10
                done
                echo 'timed out waiting for docker'
                exit 1
            ",
        }, new CustomResourceOptions
        {
            DependsOn = { instance },
        });
    
    return new Dictionary<string, object?>
    {
        ["instanceId"] = instance.Id,
        ["publicElasticIp"] = eipAssoc.PublicIp,
    };
});